#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
source_env
require_ssh_key

need_cmd ssh
SSH_KEY="$(ssh_key_path)"
remote_root="$(remote_bundle_dir)"

uninstall_remote_node() {
  local role="$1"
  local user="$2"
  local host="$3"

  echo "${role} ${host}: uninstalling k3s"
  ssh -T -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${user}@${host}" \
    "REMOTE_ROOT='$remote_root' bash -s" <<'REMOTE'
set -euo pipefail

if ! sudo -n true >/dev/null 2>&1; then
  echo "ERROR: passwordless sudo required for cleanup"
  exit 1
fi

if [[ -f "$REMOTE_ROOT/registries.yaml" ]]; then
  sudo mkdir -p /etc/rancher/k3s
  sudo install -m 0644 "$REMOTE_ROOT/registries.yaml" /etc/rancher/k3s/registries.yaml
fi

if [[ -x /usr/local/bin/k3s-agent-uninstall.sh ]]; then
  sudo /usr/local/bin/k3s-agent-uninstall.sh || true
fi
if [[ -x /usr/local/bin/k3s-uninstall.sh ]]; then
  sudo /usr/local/bin/k3s-uninstall.sh || true
fi

sudo systemctl stop k3s 2>/dev/null || true
sudo systemctl stop k3s-agent 2>/dev/null || true
sudo systemctl disable k3s 2>/dev/null || true
sudo systemctl disable k3s-agent 2>/dev/null || true

if [[ -f "$REMOTE_ROOT/registries.yaml" ]]; then
  sudo mkdir -p /etc/rancher/k3s
  sudo install -m 0644 "$REMOTE_ROOT/registries.yaml" /etc/rancher/k3s/registries.yaml
fi

echo "Preserved offline seed assets in $REMOTE_ROOT"
REMOTE
}

for entry in ${WORKER_IPS:-}; do
  [[ -n "$entry" ]] || continue
  read -r user host <<<"$(parse_node_entry "$entry" "$WORKER_SSH_USER")"
  uninstall_remote_node "worker" "$user" "$host"
done
for entry in $SERVER_IPS; do
  [[ -n "$entry" ]] || continue
  read -r user host <<<"$(parse_node_entry "$entry" "$SERVER_SSH_USER")"
  uninstall_remote_node "server" "$user" "$host"
done

rm -f "$KUBECONFIG_PATH"
echo "Removed project kubeconfig: $KUBECONFIG_PATH"
echo "Preserved local offline cache:"
echo " - bundle: $(bundle_dir)"
echo " - registry data: $(registry_data_dir)"
