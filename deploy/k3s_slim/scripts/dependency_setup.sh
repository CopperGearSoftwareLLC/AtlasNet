#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="${ROOT_DIR}/.env"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

[[ -f "$ENV_FILE" ]] || die "Missing .env at $ENV_FILE"
# shellcheck disable=SC1090
source "$ENV_FILE"

: "${SERVER_IP:?Set SERVER_IP in .env}"
: "${SERVER_SSH_USER:?Set SERVER_SSH_USER in .env}"
: "${SSH_KEY:=${HOME}/.ssh/id_ed25519}"
: "${WORKER_IPS:=}"
: "${WORKER_SSH_USER:=pi}"

SSH_KEY="${SSH_KEY/#\~/$HOME}"
[[ -f "$SSH_KEY" ]] || die "SSH key file does not exist: $SSH_KEY"

need_cmd ssh

# Install iptables and (on Pi) add cgroup flags to kernel cmdline.
# Run once per node; reboot nodes afterward for cgroup changes to take effect.
run_on_node() {
  local user="$1"
  local host="$2"
  echo "==> Setting up dependencies on ${user}@${host} ..."

  ssh -o StrictHostKeyChecking=accept-new -i "$SSH_KEY" "${user}@${host}" bash -s -- <<'REMOTE'
set -euo pipefail

# Install iptables if missing (required by k3s)
if ! command -v iptables-save >/dev/null 2>&1; then
  echo "  Installing iptables..."
  sudo apt-get update -qq
  sudo apt-get install -y iptables iptables-persistent || sudo apt-get install -y iptables
else
  echo "  iptables already present."
fi

# Add cgroup memory flags for k3s (Raspberry Pi / Bookworm-style)
for cmdline in /boot/firmware/cmdline.txt /boot/cmdline.txt; do
  if [[ -f "$cmdline" ]]; then
    if grep -q 'cgroup_memory=1' "$cmdline" 2>/dev/null; then
      echo "  $cmdline already has cgroup_memory=1."
    else
      echo "  Adding cgroup_memory=1 cgroup_enable=memory to $cmdline"
      # Append to the single line; preserve existing content
      sudo sed -i 's/$/ cgroup_memory=1 cgroup_enable=memory/' "$cmdline"
    fi
    break
  fi
done

echo "  Done. Reboot this node for cgroup changes to take effect (e.g. sudo reboot)."
REMOTE
}

# Server
run_on_node "$SERVER_SSH_USER" "$SERVER_IP"

# Workers
for ip in $WORKER_IPS; do
  [[ -n "$ip" ]] || continue
  run_on_node "$WORKER_SSH_USER" "$ip"
done

echo
echo "Dependency setup finished on all nodes."
echo "If you changed kernel cmdline (cgroup), reboot each node before running: make linux-pi"
