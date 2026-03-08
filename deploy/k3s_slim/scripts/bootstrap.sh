#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_KUBECONFIG_PATH="$ROOT_DIR/config/kubeconfig"
TARGET_KUBECONFIG_PATH="${K3S_KUBECONFIG_PATH:-$HOME/.kube/config}"
CONTEXT_NAME="k3s-homelab"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

CLI_SERVER_IPS=""
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
    --server-ips)
      [[ $# -ge 2 ]] || die "--server-ips requires a value"
      CLI_SERVER_IPS="$2"
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

[[ -n "$CLI_SERVER_IPS" ]] && SERVER_IPS="$CLI_SERVER_IPS"
[[ -n "$CLI_SERVER_SSH_USER" ]] && SERVER_SSH_USER="$CLI_SERVER_SSH_USER"
[[ -n "$CLI_SSH_KEY" ]] && SSH_KEY="$CLI_SSH_KEY"
[[ -n "$CLI_WORKER_SSH_USER" ]] && WORKER_SSH_USER="$CLI_WORKER_SSH_USER"
[[ -n "$CLI_WORKER_IPS" ]] && WORKER_IPS="$CLI_WORKER_IPS"
[[ -n "$CLI_K3S_API_ENDPOINT" ]] && K3S_API_ENDPOINT="$CLI_K3S_API_ENDPOINT"
[[ -n "$CLI_K3S_EXTRA_ARGS" ]] && K3S_EXTRA_ARGS="$CLI_K3S_EXTRA_ARGS"
[[ -n "$CLI_K3S_VERSION" ]] && K3S_VERSION="$CLI_K3S_VERSION"
[[ -n "$CLI_K3SUP_VERSION" ]] && K3SUP_VERSION="$CLI_K3SUP_VERSION"
[[ -n "$CLI_K3SUP_USE_SUDO" ]] && K3SUP_USE_SUDO="$CLI_K3SUP_USE_SUDO"

# At least one server required
: "${SERVER_IPS:?At least one server required (set SERVER_IPS in .env)}"
: "${SERVER_SSH_USER:?SERVER_SSH_USER is required (pass --server-user from Makefile/k3s-deploy)}"
: "${SSH_KEY:?SSH_KEY is required (pass --ssh-key from Makefile/k3s-deploy)}"
: "${WORKER_SSH_USER:=pi}"
: "${WORKER_IPS:=}"
: "${K3S_EXTRA_ARGS:=}"
: "${K3S_VERSION:=}"
: "${K3SUP_VERSION:=}"
: "${K3SUP_USE_SUDO:=true}"

# Derive primary server entry (may be \"user@ip\" or just \"ip\")
PRIMARY_SERVER_ENTRY="${SERVER_IPS%% *}"
# Strip any stray double quotes that might sneak in from .env formatting
PRIMARY_SERVER_ENTRY="${PRIMARY_SERVER_ENTRY//\"/}"
if [[ "$PRIMARY_SERVER_ENTRY" == *@* ]]; then
  PRIMARY_SERVER_USER_DEFAULT="${PRIMARY_SERVER_ENTRY%@*}"
  PRIMARY_SERVER_IP="${PRIMARY_SERVER_ENTRY#*@}"
else
  PRIMARY_SERVER_USER_DEFAULT="$SERVER_SSH_USER"
  PRIMARY_SERVER_IP="$PRIMARY_SERVER_ENTRY"
fi
: "${K3S_API_ENDPOINT:=$PRIMARY_SERVER_IP}"

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

ensure_remote_curl() {
  local host_ip="$1"
  local host_user="$2"
  local label="$3"

  if ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${host_user}@${host_ip}" 'command -v curl >/dev/null 2>&1'; then
    return 0
  fi

  echo "${label}: curl not found; installing..."
  ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${host_user}@${host_ip}" '
      set -euo pipefail
      if command -v apt-get >/dev/null 2>&1; then
        sudo apt-get update -qq
        sudo apt-get install -y curl
      else
        echo "ERROR: curl is missing and apt-get is unavailable on this node." >&2
        exit 1
      fi
    '
}

check_server_api_port_conflict() {
  local server_ip="$1"
  local listeners
  listeners="$(
    ssh -i "$SSH_KEY" \
      -o BatchMode=yes \
      -o StrictHostKeyChecking=accept-new \
      -o ConnectTimeout=5 \
      "${SERVER_SSH_USER}@${server_ip}" \
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
ERROR: Server API port 6443 is already in use on ${server_ip}.
Current listeners:
${listeners}

This blocks k3s server startup.
Common cause on dev machines: IDE/port-forward process bound to 127.0.0.1:6443.
Close/stop that port forward and rerun 'make k3s-deploy'.
EOF2
  exit 1
}

install_k3sup_if_missing() {
  local k3sup_bin="/usr/local/bin/k3sup"

  if [[ -x "$k3sup_bin" ]]; then
    export PATH="/usr/local/bin:$PATH"
    return 0
  fi

  echo "k3sup not found at $k3sup_bin; downloading installer payload (requires curl)..."
  need_cmd curl

  # Use the official installer flow:
  #   curl -sLS https://get.k3sup.dev | sh
  #   sudo install k3sup /usr/local/bin/
  if [[ -n "$K3SUP_VERSION" ]]; then
    curl -sLS https://get.k3sup.dev | sh -s -- "$K3SUP_VERSION"
  else
    curl -sLS https://get.k3sup.dev | sh
  fi

  local k3sup_src=""
  if [[ -f "$PWD/k3sup" ]]; then
    k3sup_src="$PWD/k3sup"
  elif [[ -f "$ROOT_DIR/k3sup" ]]; then
    k3sup_src="$ROOT_DIR/k3sup"
  fi

  [[ -n "$k3sup_src" ]] || die "k3sup download completed but binary was not found (expected ./k3sup)."
  chmod +x "$k3sup_src"

  if install -m 0755 "$k3sup_src" "$k3sup_bin" 2>/dev/null; then
    :
  elif command -v sudo >/dev/null 2>&1; then
    sudo install -m 0755 "$k3sup_src" "$k3sup_bin"
  else
    die "Cannot write /usr/local/bin and sudo is unavailable. Install k3sup manually: sudo install $k3sup_src $k3sup_bin"
  fi

  export PATH="/usr/local/bin:$PATH"
  [[ -x "$k3sup_bin" ]] || die "k3sup install failed"
}

K3SUP_RETRIES="${K3SUP_RETRIES:-3}"
K3SUP_RETRY_SLEEP_SECS="${K3SUP_RETRY_SLEEP_SECS:-5}"

k3sup_retry() {
  local attempt=1
  local max="$K3SUP_RETRIES"
  local sleep_s="$K3SUP_RETRY_SLEEP_SECS"

  while true; do
    if k3sup "$@"; then
      return 0
    fi

    if [[ "$attempt" -ge "$max" ]]; then
      cat >&2 <<EOF2
ERROR: k3sup failed after ${attempt} attempt(s).

This is often caused by upstream download errors (for example GitHub release endpoints returning 5xx)
while fetching k3s binaries/hashes. If you saw a "Download failed" message, try rerunning later,
or set K3S_VERSION in your .env to a pinned version and retry.
EOF2
      return 1
    fi

    echo "WARN: k3sup failed (attempt ${attempt}/${max}). Retrying in ${sleep_s}s..." >&2
    sleep "$sleep_s"
    attempt=$((attempt + 1))
  done
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
for entry in $SERVER_IPS; do
  [[ -n "$entry" ]] || continue
  entry="${entry//\"/}"
  if [[ "$entry" == *@* ]]; then
    user="${entry%@*}"
    host="${entry#*@}"
  else
    user="$SERVER_SSH_USER"
    host="$entry"
  fi
  check_ssh_access "$host" "$user"
done
for entry in $WORKER_IPS; do
  [[ -n "$entry" ]] || continue
  entry="${entry//\"/}"
  if [[ "$entry" == *@* ]]; then
    user="${entry%@*}"
    host="${entry#*@}"
  else
    user="$WORKER_SSH_USER"
    host="$entry"
  fi
  check_ssh_access "$host" "$user"
done

echo "Checking sudo access on nodes ..."
for entry in $SERVER_IPS; do
  [[ -n "$entry" ]] || continue
  entry="${entry//\"/}"
  if [[ "$entry" == *@* ]]; then
    user="${entry%@*}"
    host="${entry#*@}"
  else
    user="$SERVER_SSH_USER"
    host="$entry"
  fi
  check_sudo_access "$host" "$user"
done
for entry in $WORKER_IPS; do
  [[ -n "$entry" ]] || continue
  entry="${entry//\"/}"
  if [[ "$entry" == *@* ]]; then
    user="${entry%@*}"
    host="${entry#*@}"
  else
    user="$WORKER_SSH_USER"
    host="$entry"
  fi
  check_sudo_access "$host" "$user"
done

echo "Ensuring curl is available on nodes ..."
for entry in $SERVER_IPS; do
  [[ -n "$entry" ]] || continue
  entry="${entry//\"/}"
  if [[ "$entry" == *@* ]]; then
    user="${entry%@*}"
    host="${entry#*@}"
  else
    user="$SERVER_SSH_USER"
    host="$entry"
  fi
  ensure_remote_curl "$host" "$user" "server ${host}"
done
for entry in $WORKER_IPS; do
  [[ -n "$entry" ]] || continue
  entry="${entry//\"/}"
  if [[ "$entry" == *@* ]]; then
    user="${entry%@*}"
    host="${entry#*@}"
  else
    user="$WORKER_SSH_USER"
    host="$entry"
  fi
  ensure_remote_curl "$host" "$user" "worker ${host}"
done

check_server_api_port_conflict "$PRIMARY_SERVER_IP"

install_k3sup_if_missing

echo "Installing k3s server on $PRIMARY_SERVER_IP (first server) ..."
K3S_SERVER_ARGS="--write-kubeconfig-mode 644"
if [[ -n "$K3S_EXTRA_ARGS" ]]; then
  K3S_SERVER_ARGS+=" ${K3S_EXTRA_ARGS}"
fi

K3SUP_INSTALL_ARGS=(
  install
  --ip "$PRIMARY_SERVER_IP"
  --user "$PRIMARY_SERVER_USER_DEFAULT"
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
k3sup_retry "${K3SUP_INSTALL_ARGS[@]}"

sync_project_kubeconfig_copy

export KUBECONFIG="$TARGET_KUBECONFIG_PATH"

# Join additional servers (HA) if any
SERVER_IPS_ARR=($SERVER_IPS)
if [[ ${#SERVER_IPS_ARR[@]} -gt 1 ]]; then
  echo
  echo "Joining additional server(s) ..."
  join_pids=()
  join_labels=()
  for ip in "${SERVER_IPS_ARR[@]:1}"; do
    [[ -n "$ip" ]] || continue
    ip="${ip//\"/}"
    if [[ "$ip" == *@* ]]; then
      join_user="${ip%@*}"
      join_host="${ip#*@}"
    else
      join_user="$SERVER_SSH_USER"
      join_host="$ip"
    fi
    echo " - server: $join_user@$join_host"
    k3sup_retry join \
      --server-ip "$PRIMARY_SERVER_IP" \
      --server-user "$PRIMARY_SERVER_USER_DEFAULT" \
      --ip "$join_host" \
      --user "$join_user" \
      --server \
      --sudo "$K3SUP_USE_SUDO" \
      --ssh-key "$SSH_KEY" &
    join_pids+=($!)
    join_labels+=("server $join_user@$join_host")
  done

  join_failed=0
  for i in "${!join_pids[@]}"; do
    pid="${join_pids[$i]}"
    label="${join_labels[$i]}"
    if ! wait "$pid"; then
      echo "ERROR: k3sup join failed for ${label}" >&2
      join_failed=1
    fi
  done
  if [[ "$join_failed" -ne 0 ]]; then
    die "One or more additional server joins failed."
  fi
fi

echo
echo "Joining workers (if any) ..."
if [[ -z "${WORKER_IPS// }" ]]; then
  echo " - no workers configured"
else
  worker_pids=()
  worker_labels=()
  for entry in $WORKER_IPS; do
    [[ -n "$entry" ]] || continue
    entry="${entry//\"/}"
    if [[ "$entry" == *@* ]]; then
      worker_user="${entry%@*}"
      worker_host="${entry#*@}"
    else
      worker_user="$WORKER_SSH_USER"
      worker_host="$entry"
    fi
    echo " - worker: $worker_user@$worker_host"
    k3sup_retry join \
      --server-ip "$PRIMARY_SERVER_IP" \
      --server-user "$PRIMARY_SERVER_USER_DEFAULT" \
      --ip "$worker_host" \
      --user "$worker_user" \
      --sudo "$K3SUP_USE_SUDO" \
      --ssh-key "$SSH_KEY" &
    worker_pids+=($!)
    worker_labels+=("worker $worker_user@$worker_host")
  done

  worker_failed=0
  for i in "${!worker_pids[@]}"; do
    pid="${worker_pids[$i]}"
    label="${worker_labels[$i]}"
    if ! wait "$pid"; then
      echo "ERROR: k3sup join failed for ${label}" >&2
      worker_failed=1
    fi
  done
  if [[ "$worker_failed" -ne 0 ]]; then
    die "One or more worker joins failed."
  fi
fi

echo "Cluster context: $CONTEXT_NAME"
kubectl config use-context "$CONTEXT_NAME" >/dev/null

echo
echo "Done. Verify with:"
echo "  export KUBECONFIG=\"$TARGET_KUBECONFIG_PATH\""
echo "  kubectl get nodes -o wide"
echo
kubectl get nodes -o wide || true
