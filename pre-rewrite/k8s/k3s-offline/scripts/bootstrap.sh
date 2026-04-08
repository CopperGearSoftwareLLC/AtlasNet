#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
source_env
require_ssh_key

need_cmd ssh
need_cmd scp
need_cmd kubectl

: "${SERVER_IPS:?Set SERVER_IPS in .env}"
: "${SERVER_SSH_USER:?Set SERVER_SSH_USER in .env}"
: "${WORKER_IPS:=}"
: "${WORKER_SSH_USER:=pi}"
: "${K3S_EXTRA_ARGS:=}"
: "${K3S_AGENT_EXTRA_ARGS:=}"
: "${K3S_VERSION:?Set K3S_VERSION in .env}"
: "${K3S_INSTALL_START_TIMEOUT:=120s}"

remote_root="$(remote_bundle_dir)"
SSH_KEY="$(ssh_key_path)"
TARGET_KUBECONFIG_PATH="${K3S_KUBECONFIG_PATH:-$HOME/.kube/config}"

primary_entry="${SERVER_IPS%% *}"
read -r PRIMARY_SERVER_USER PRIMARY_SERVER_IP <<<"$(parse_node_entry "$primary_entry" "$SERVER_SSH_USER")"
: "${K3S_API_ENDPOINT:=$PRIMARY_SERVER_IP}"

mkdir -p "$(dirname "$TARGET_KUBECONFIG_PATH")" "$(dirname "$KUBECONFIG_PATH")"

ensure_offline_registry_arg() {
  local args="$1"
  if [[ " $args " != *" --disable-default-registry-endpoint "* ]]; then
    if [[ -n "$args" ]]; then
      args+=" "
    fi
    args+="--disable-default-registry-endpoint"
  fi
  printf '%s\n' "$args"
}

ensure_disable_arg() {
  local args="$1"
  local component="$2"

  if [[ " $args " != *" --disable ${component} "* && " $args " != *" --disable=${component} "* ]]; then
    if [[ -n "$args" ]]; then
      args+=" "
    fi
    args+="--disable ${component}"
  fi

  printf '%s\n' "$args"
}

ensure_packaged_component_disables() {
  local args="$1"

  if [[ "${INSTALL_METRICS_SERVER:-true}" == "true" ]]; then
    args="$(ensure_disable_arg "$args" "metrics-server")"
  fi
  if [[ "${INSTALL_INGRESS_NGINX:-false}" == "true" ]]; then
    args="$(ensure_disable_arg "$args" "traefik")"
  fi
  if [[ "${INSTALL_METALLB:-false}" == "true" || "${INSTALL_INGRESS_NGINX:-false}" == "true" ]]; then
    args="$(ensure_disable_arg "$args" "servicelb")"
  fi

  printf '%s\n' "$args"
}

check_access() {
  local role="$1"
  local user="$2"
  local host="$3"

  ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${user}@${host}" true >/dev/null 2>&1 || \
    die "SSH auth failed for ${user}@${host}. Run 'make ssh-setup' first."

  ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${user}@${host}" 'sudo -n true' >/dev/null 2>&1 || \
    die "Passwordless sudo is required for ${role} ${host}. Run 'make sudo-setup' first."
}

verify_seeded_assets() {
  local role="$1"
  local user="$2"
  local host="$3"
  local arch airgap_name

  arch="$(normalize_arch "$(ssh -i "$SSH_KEY" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o ConnectTimeout=5 "${user}@${host}" 'uname -m')")"
  airgap_name="$(k3s_airgap_asset "$arch")"

  ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${user}@${host}" \
    "test -f '$remote_root/install.sh' && test -f '$remote_root/bin/k3s' && test -f '$remote_root/$airgap_name' && test -f '/etc/rancher/k3s/registries.yaml'" >/dev/null || \
    die "Missing seeded offline assets on ${role} ${host}. Run 'make dependency-setup' while online first."
}

wait_for_primary_server_ready() {
  local attempts="${1:-60}"
  local sleep_seconds="${2:-2}"
  local attempt

  echo "Waiting for primary k3s server API on ${PRIMARY_SERVER_IP} ..."
  for attempt in $(seq 1 "$attempts"); do
    if ssh -i "$SSH_KEY" \
      -o BatchMode=yes \
      -o StrictHostKeyChecking=accept-new \
      -o ConnectTimeout=5 \
      "${PRIMARY_SERVER_USER}@${PRIMARY_SERVER_IP}" \
      'sudo systemctl is-active k3s >/dev/null 2>&1 && sudo k3s kubectl get --raw=/readyz >/dev/null 2>&1'; then
      return 0
    fi
    sleep "$sleep_seconds"
  done

  echo "Primary server did not report ready in time. Recent k3s logs:" >&2
  ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${PRIMARY_SERVER_USER}@${PRIMARY_SERVER_IP}" \
    'sudo systemctl status k3s --no-pager --full || true; echo; sudo journalctl -u k3s --no-pager -n 120 || true' >&2 || true
  die "Primary k3s server failed to become ready."
}

install_k3s_node() {
  local mode="$1"
  local user="$2"
  local host="$3"
  local extra_args="$4"
  local k3s_url="${5:-}"
  local k3s_token="${6:-}"
  local arch airgap_name exec_args service_name

  arch="$(normalize_arch "$(ssh -i "$SSH_KEY" -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o ConnectTimeout=5 "${user}@${host}" 'uname -m')")"
  airgap_name="$(k3s_airgap_asset "$arch")"
  service_name="k3s"
  [[ "$mode" == "agent" ]] && service_name="k3s-agent"

  exec_args="$mode"
  if [[ "$mode" == "server" && -z "$k3s_url" ]]; then
    exec_args+=" --write-kubeconfig-mode 644 --tls-san ${K3S_API_ENDPOINT}"
  fi
  if [[ -n "$extra_args" ]]; then
    exec_args+=" ${extra_args}"
  fi
  if [[ "$mode" == "server" ]]; then
    exec_args="$(ensure_packaged_component_disables "$exec_args")"
  fi
  exec_args="$(ensure_offline_registry_arg "$exec_args")"

  echo "==> Installing k3s ${mode} on ${user}@${host} (${arch})"
  ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${user}@${host}" \
    "REMOTE_ROOT='$remote_root' AIRGAP_NAME='$airgap_name' INSTALL_EXEC='$exec_args' JOIN_URL='$k3s_url' JOIN_TOKEN='$k3s_token' K3S_VERSION='$K3S_VERSION' INSTALL_START_TIMEOUT='$K3S_INSTALL_START_TIMEOUT' SERVICE_NAME='$service_name' bash -s" <<'REMOTE'
set -euo pipefail

sudo install -m 0755 "$REMOTE_ROOT/bin/k3s" /usr/local/bin/k3s
sudo mkdir -p /var/lib/rancher/k3s/agent/images
sudo cp "$REMOTE_ROOT/$AIRGAP_NAME" "/var/lib/rancher/k3s/agent/images/$AIRGAP_NAME"
sudo chmod 0644 "/var/lib/rancher/k3s/agent/images/$AIRGAP_NAME"

status=0
if [[ -n "$JOIN_URL" ]]; then
  if ! timeout "$INSTALL_START_TIMEOUT" \
    sudo env \
      INSTALL_K3S_SKIP_DOWNLOAD=true \
      INSTALL_K3S_VERSION="$K3S_VERSION" \
      INSTALL_K3S_EXEC="$INSTALL_EXEC" \
      K3S_URL="$JOIN_URL" \
      K3S_TOKEN="$JOIN_TOKEN" \
      sh "$REMOTE_ROOT/install.sh"; then
    status=$?
  fi
else
  if ! timeout "$INSTALL_START_TIMEOUT" \
    sudo env \
      INSTALL_K3S_SKIP_DOWNLOAD=true \
      INSTALL_K3S_VERSION="$K3S_VERSION" \
      INSTALL_K3S_EXEC="$INSTALL_EXEC" \
      sh "$REMOTE_ROOT/install.sh"; then
    status=$?
  fi
fi

if [[ "$status" -eq 0 ]]; then
  for _ in $(seq 1 20); do
    if sudo systemctl is-active "$SERVICE_NAME" >/dev/null 2>&1; then
      break
    fi
    sleep 1
  done
  if ! sudo systemctl is-active "$SERVICE_NAME" >/dev/null 2>&1; then
    status=1
  fi
fi

if [[ "$status" -ne 0 ]]; then
  echo "ERROR: ${SERVICE_NAME} install/start did not complete successfully on this node." >&2
  sudo systemctl status "$SERVICE_NAME" --no-pager --full || true
  echo >&2
  sudo journalctl -u "$SERVICE_NAME" --no-pager -n 120 || true
  exit "$status"
fi

echo "${SERVICE_NAME} install/start completed."
REMOTE
}

for_each_node check_access
for_each_node verify_seeded_assets

install_k3s_node "server" "$PRIMARY_SERVER_USER" "$PRIMARY_SERVER_IP" "$K3S_EXTRA_ARGS"
wait_for_primary_server_ready

K3S_TOKEN="$(ssh -i "$SSH_KEY" \
  -o BatchMode=yes \
  -o StrictHostKeyChecking=accept-new \
  -o ConnectTimeout=5 \
  "${PRIMARY_SERVER_USER}@${PRIMARY_SERVER_IP}" \
  'sudo cat /var/lib/rancher/k3s/server/node-token')"

server_count=0
for entry in $SERVER_IPS; do
  [[ -n "$entry" ]] || continue
  server_count=$((server_count + 1))
  [[ "$server_count" -eq 1 ]] && continue
  read -r user host <<<"$(parse_node_entry "$entry" "$SERVER_SSH_USER")"
  install_k3s_node "server" "$user" "$host" "$K3S_EXTRA_ARGS" "https://${K3S_API_ENDPOINT}:6443" "$K3S_TOKEN"
done

for entry in ${WORKER_IPS:-}; do
  [[ -n "$entry" ]] || continue
  read -r user host <<<"$(parse_node_entry "$entry" "$WORKER_SSH_USER")"
  install_k3s_node "agent" "$user" "$host" "$K3S_AGENT_EXTRA_ARGS" "https://${K3S_API_ENDPOINT}:6443" "$K3S_TOKEN"
done

ssh -i "$SSH_KEY" \
  -o BatchMode=yes \
  -o StrictHostKeyChecking=accept-new \
  -o ConnectTimeout=5 \
  "${PRIMARY_SERVER_USER}@${PRIMARY_SERVER_IP}" \
  'sudo cat /etc/rancher/k3s/k3s.yaml' > "$TARGET_KUBECONFIG_PATH"

sed -i "s/127.0.0.1/${K3S_API_ENDPOINT}/g" "$TARGET_KUBECONFIG_PATH"
cp "$TARGET_KUBECONFIG_PATH" "$KUBECONFIG_PATH"
chmod 600 "$TARGET_KUBECONFIG_PATH" "$KUBECONFIG_PATH"

export KUBECONFIG="$KUBECONFIG_PATH"
echo
kubectl get nodes -o wide || true
