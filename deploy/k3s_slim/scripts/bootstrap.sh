#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_KUBECONFIG_PATH="$ROOT_DIR/config/kubeconfig"
TARGET_KUBECONFIG_PATH="${K3S_KUBECONFIG_PATH:-$HOME/.kube/config}"
CONTEXT_NAME="k3s-homelab"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

CLI_SERVER_IP=""
CLI_SERVER_SSH_USER=""
CLI_SSH_KEY=""
CLI_WORKER_SSH_USER=""
CLI_K3S_API_ENDPOINT=""
CLI_WORKER_IPS=""
CLI_K3S_EXTRA_ARGS=""
CLI_K3S_VERSION=""
CLI_K3SUP_VERSION=""
CLI_K3SUP_USE_SUDO=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --server-ip)
      [[ $# -ge 2 ]] || die "--server-ip requires a value"
      CLI_SERVER_IP="$2"
      shift 2
      ;;
    --server-user)
      [[ $# -ge 2 ]] || die "--server-user requires a value"
      CLI_SERVER_SSH_USER="$2"
      shift 2
      ;;
    --ssh-key)
      [[ $# -ge 2 ]] || die "--ssh-key requires a value"
      CLI_SSH_KEY="$2"
      shift 2
      ;;
    --use-sudo)
      [[ $# -ge 2 ]] || die "--use-sudo requires a value (true|false)"
      CLI_K3SUP_USE_SUDO="$2"
      shift 2
      ;;
    --worker-ip)
      [[ $# -ge 2 ]] || die "--worker-ip requires a value"
      CLI_WORKER_IPS="${CLI_WORKER_IPS} $2"
      shift 2
      ;;
    --worker-ips)
      [[ $# -ge 2 ]] || die "--worker-ips requires a value"
      CLI_WORKER_IPS="$2"
      shift 2
      ;;
    --worker-user)
      [[ $# -ge 2 ]] || die "--worker-user requires a value"
      CLI_WORKER_SSH_USER="$2"
      shift 2
      ;;
    --api-endpoint)
      [[ $# -ge 2 ]] || die "--api-endpoint requires a value"
      CLI_K3S_API_ENDPOINT="$2"
      shift 2
      ;;
    --k3s-extra-args)
      [[ $# -ge 2 ]] || die "--k3s-extra-args requires a value"
      CLI_K3S_EXTRA_ARGS="$2"
      shift 2
      ;;
    --k3s-version)
      [[ $# -ge 2 ]] || die "--k3s-version requires a value"
      CLI_K3S_VERSION="$2"
      shift 2
      ;;
    --k3sup-version)
      [[ $# -ge 2 ]] || die "--k3sup-version requires a value"
      CLI_K3SUP_VERSION="$2"
      shift 2
      ;;
    *)
      die "Unknown option: $1 (run with --help)"
      ;;
  esac
done

[[ -n "$CLI_SERVER_IP" ]] && SERVER_IP="$CLI_SERVER_IP"
[[ -n "$CLI_SERVER_SSH_USER" ]] && SERVER_SSH_USER="$CLI_SERVER_SSH_USER"
[[ -n "$CLI_SSH_KEY" ]] && SSH_KEY="$CLI_SSH_KEY"
[[ -n "$CLI_WORKER_SSH_USER" ]] && WORKER_SSH_USER="$CLI_WORKER_SSH_USER"
[[ -n "$CLI_WORKER_IPS" ]] && WORKER_IPS="$CLI_WORKER_IPS"
[[ -n "$CLI_K3S_API_ENDPOINT" ]] && K3S_API_ENDPOINT="$CLI_K3S_API_ENDPOINT"
[[ -n "$CLI_K3S_EXTRA_ARGS" ]] && K3S_EXTRA_ARGS="$CLI_K3S_EXTRA_ARGS"
[[ -n "$CLI_K3S_VERSION" ]] && K3S_VERSION="$CLI_K3S_VERSION"
[[ -n "$CLI_K3SUP_VERSION" ]] && K3SUP_VERSION="$CLI_K3SUP_VERSION"
[[ -n "$CLI_K3SUP_USE_SUDO" ]] && K3SUP_USE_SUDO="$CLI_K3SUP_USE_SUDO"

: "${SERVER_IP:?SERVER_IP is required (pass --server-ip from Makefile/linux-pi)}"
: "${SERVER_SSH_USER:?SERVER_SSH_USER is required (pass --server-user from Makefile/linux-pi)}"
: "${SSH_KEY:?SSH_KEY is required (pass --ssh-key from Makefile/linux-pi)}"
: "${WORKER_SSH_USER:=pi}"
: "${WORKER_IPS:=}"
: "${K3S_EXTRA_ARGS:=}"
: "${K3S_VERSION:=}"
: "${K3SUP_VERSION:=}"
: "${K3S_API_ENDPOINT:=$SERVER_IP}"
: "${K3SUP_USE_SUDO:=true}"

[[ "$K3SUP_USE_SUDO" == "true" || "$K3SUP_USE_SUDO" == "false" ]] || \
  die "K3SUP_USE_SUDO must be 'true' or 'false' (got: $K3SUP_USE_SUDO)"

SSH_KEY="${SSH_KEY/#\~/$HOME}"
[[ -f "$SSH_KEY" ]] || die "SSH key file does not exist: $SSH_KEY"

need_cmd ssh
need_cmd kubectl
# Helm is only required for platform.sh, not for bootstrap.
# k3sup can be auto-installed below.

check_ssh_access() {
  local host_ip="$1"
  local host_user="$2"

  if ! ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${host_user}@${host_ip}" true >/dev/null 2>&1; then
    die "SSH auth failed for ${host_user}@${host_ip}. Install your public key on that node first (example: ssh-copy-id -i ${SSH_KEY}.pub ${host_user}@${host_ip})"
  fi
}

check_sudo_access() {
  local host_ip="$1"
  local host_user="$2"

  [[ "$K3SUP_USE_SUDO" == "true" ]] || return 0

  if ! ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${host_user}@${host_ip}" 'sudo -n true' >/dev/null 2>&1; then
    die "Passwordless sudo is required for ${host_user}@${host_ip}. Run 'make sudo-setup', or set it manually in /etc/sudoers.d, or set K3SUP_USE_SUDO=false and use root SSH."
  fi
}

check_server_api_port_conflict() {
  local listeners
  listeners="$(
    ssh -i "$SSH_KEY" \
      -o BatchMode=yes \
      -o StrictHostKeyChecking=accept-new \
      -o ConnectTimeout=5 \
      "${SERVER_SSH_USER}@${SERVER_IP}" \
      "sudo ss -ltnp '( sport = :6443 )' 2>/dev/null | awk 'NR>1 {print}'" \
      2>/dev/null || true
  )"

  if [[ -z "${listeners// }" ]]; then
    return 0
  fi
  if [[ "$listeners" == *k3s* ]]; then
    return 0
  fi

  cat >&2 <<EOF2
ERROR: Server API port 6443 is already in use on ${SERVER_IP}.
Current listeners:
${listeners}

This blocks k3s server startup.
Common cause on dev machines: IDE/port-forward process bound to 127.0.0.1:6443.
Close/stop that port forward and rerun 'make linux-pi'.
EOF2
  exit 1
}

install_k3sup_if_missing() {
  if command -v k3sup >/dev/null 2>&1; then
    return 0
  fi

  echo "k3sup not found; installing to ~/.local/bin (requires curl)..."
  need_cmd curl
  mkdir -p "$HOME/.local/bin"
  export PATH="$HOME/.local/bin:$PATH"

  # Official installer script from the k3sup project (convenient for learning/homelab).
  # If you prefer, install a pinned binary manually and remove this.
  if [[ -n "$K3SUP_VERSION" ]]; then
    curl -sLS https://get.k3sup.dev | sh -s -- -b "$HOME/.local/bin" "$K3SUP_VERSION"
  else
    curl -sLS https://get.k3sup.dev | sh -s -- -b "$HOME/.local/bin"
  fi

  command -v k3sup >/dev/null 2>&1 || die "k3sup install failed"
}

sync_project_kubeconfig_copy() {
  mkdir -p "$(dirname "$PROJECT_KUBECONFIG_PATH")"
  if [[ "$TARGET_KUBECONFIG_PATH" != "$PROJECT_KUBECONFIG_PATH" ]]; then
    cp "$TARGET_KUBECONFIG_PATH" "$PROJECT_KUBECONFIG_PATH"
  fi
  chmod 600 "$TARGET_KUBECONFIG_PATH" >/dev/null 2>&1 || true
  chmod 600 "$PROJECT_KUBECONFIG_PATH" >/dev/null 2>&1 || true
  echo "Kubeconfig target: $TARGET_KUBECONFIG_PATH"
  echo "Project kubeconfig copy: $PROJECT_KUBECONFIG_PATH"
}

prepare_target_kubeconfig_path() {
  local target_dir
  local previous_real_path=""
  target_dir="$(dirname "$TARGET_KUBECONFIG_PATH")"
  mkdir -p "$target_dir"

  # If an older run left ~/.kube/config as a symlink, replace it with a real file path target.
  if [[ -L "$TARGET_KUBECONFIG_PATH" ]]; then
    previous_real_path="$(readlink -f "$TARGET_KUBECONFIG_PATH" 2>/dev/null || true)"
    rm -f "$TARGET_KUBECONFIG_PATH"
    if [[ -n "$previous_real_path" && -f "$previous_real_path" ]]; then
      cp "$previous_real_path" "$TARGET_KUBECONFIG_PATH"
    else
      : >"$TARGET_KUBECONFIG_PATH"
    fi
  fi
}

echo "Checking SSH access to nodes ..."
check_ssh_access "$SERVER_IP" "$SERVER_SSH_USER"
for ip in $WORKER_IPS; do
  check_ssh_access "$ip" "$WORKER_SSH_USER"
done

echo "Checking sudo access on nodes ..."
check_sudo_access "$SERVER_IP" "$SERVER_SSH_USER"
for ip in $WORKER_IPS; do
  check_sudo_access "$ip" "$WORKER_SSH_USER"
done

check_server_api_port_conflict

install_k3sup_if_missing

echo "Installing k3s server on $SERVER_IP ..."
K3S_SERVER_ARGS="--write-kubeconfig-mode 644"
if [[ -n "$K3S_EXTRA_ARGS" ]]; then
  K3S_SERVER_ARGS+=" ${K3S_EXTRA_ARGS}"
fi

K3SUP_INSTALL_ARGS=(
  install
  --ip "$SERVER_IP"
  --user "$SERVER_SSH_USER"
  --ssh-key "$SSH_KEY"
  --tls-san "$K3S_API_ENDPOINT"
  --local-path "$TARGET_KUBECONFIG_PATH"
  --context "$CONTEXT_NAME"
  --sudo "$K3SUP_USE_SUDO"
  --k3s-extra-args "$K3S_SERVER_ARGS"
)

if [[ -n "$K3S_VERSION" ]]; then
  K3SUP_INSTALL_ARGS+=(--k3s-version "$K3S_VERSION")
fi

prepare_target_kubeconfig_path
k3sup "${K3SUP_INSTALL_ARGS[@]}"

sync_project_kubeconfig_copy

export KUBECONFIG="$TARGET_KUBECONFIG_PATH"

echo
echo "Joining workers (if any) ..."
if [[ -z "${WORKER_IPS// }" ]]; then
  echo " - no workers configured"
else
  for ip in $WORKER_IPS; do
    echo " - worker: $ip"
    k3sup join \
      --server-ip "$SERVER_IP" \
      --server-user "$SERVER_SSH_USER" \
      --ip "$ip" \
      --user "$WORKER_SSH_USER" \
      --sudo "$K3SUP_USE_SUDO" \
      --ssh-key "$SSH_KEY"
  done
fi

echo "Cluster context: $CONTEXT_NAME"
kubectl config use-context "$CONTEXT_NAME" >/dev/null

echo
echo "Done. Verify with:"
echo "  export KUBECONFIG=\"$TARGET_KUBECONFIG_PATH\""
echo "  kubectl get nodes -o wide"
