#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="$ROOT_DIR/.env"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

parse_entry() {
  local entry="$1"
  local default_user="$2"

  entry="${entry//\"/}"
  if [[ "$entry" == *@* ]]; then
    printf '%s|%s\n' "${entry%@*}" "${entry#*@}"
  else
    printf '%s|%s\n' "$default_user" "$entry"
  fi
}

extract_flannel_iface_from_args() {
  local args="${1:-}"
  local token

  for token in $args; do
    if [[ "$token" == --flannel-iface=* ]]; then
      printf '%s\n' "${token#--flannel-iface=}"
      return 0
    fi
  done

  return 1
}

if [[ -f "$ENV_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$ENV_FILE"
fi

: "${WORKER_IPS:=}"
: "${WORKER_SSH_USER:=pi}"
: "${SSH_KEY:=$HOME/.ssh/id_ed25519}"
: "${K3S_AGENT_EXTRA_ARGS:=}"
: "${K3S_AGENT_RECOVERY_WAIT_SECONDS:=8}"
: "${K3S_AGENT_RECOVERY_INTERFACE:=}"

[[ -n "${WORKER_IPS// }" ]] || die "No workers configured in WORKER_IPS."

SSH_KEY="${SSH_KEY/#\~/$HOME}"
[[ -f "$SSH_KEY" ]] || die "SSH key file does not exist: $SSH_KEY"

need_cmd ssh

RECOVERY_INTERFACE="${K3S_AGENT_RECOVERY_INTERFACE:-}"
if [[ -z "$RECOVERY_INTERFACE" ]]; then
  RECOVERY_INTERFACE="$(extract_flannel_iface_from_args "$K3S_AGENT_EXTRA_ARGS" || true)"
fi
[[ -n "$RECOVERY_INTERFACE" ]] || die "Set K3S_AGENT_RECOVERY_INTERFACE or include --flannel-iface=... in K3S_AGENT_EXTRA_ARGS."

SSH_OPTS=(
  -i "$SSH_KEY"
  -o StrictHostKeyChecking=accept-new
  -o BatchMode=yes
  -o ConnectTimeout=8
)

for entry in $WORKER_IPS; do
  [[ -n "$entry" ]] || continue

  parsed="$(parse_entry "$entry" "$WORKER_SSH_USER")"
  worker_user="${parsed%%|*}"
  worker_host="${parsed##*|}"

  echo "Installing k3s network recovery hook on ${worker_user}@${worker_host} for interface ${RECOVERY_INTERFACE} ..."
  ssh "${SSH_OPTS[@]}" "${worker_user}@${worker_host}" \
    sudo FLANNEL_IFACE="$RECOVERY_INTERFACE" RECOVERY_WAIT_SECONDS="$K3S_AGENT_RECOVERY_WAIT_SECONDS" bash -s <<'REMOTE_SCRIPT'
set -euo pipefail

install -d -m 0755 /etc/NetworkManager/dispatcher.d

cat > /etc/NetworkManager/dispatcher.d/90-k3s-agent-recover <<'SCRIPT_EOF'
#!/usr/bin/env bash
set -euo pipefail

iface="${1:-}"
action="${2:-}"
watch_iface="${FLANNEL_IFACE:-}"
wait_seconds="${RECOVERY_WAIT_SECONDS:-8}"

case "$action" in
  up|dhcp4-change|connectivity-change|reapply)
    ;;
  *)
    exit 0
    ;;
esac

[[ -n "$iface" && -n "$watch_iface" && "$iface" == "$watch_iface" ]] || exit 0

(
  sleep "$wait_seconds"

  if ! systemctl is-active --quiet k3s-agent; then
    exit 0
  fi

  if ! ip -4 addr show dev "$watch_iface" | grep -q 'inet '; then
    exit 0
  fi

  if ip link show flannel.1 >/dev/null 2>&1; then
    exit 0
  fi

  logger -t k3s-agent-recover "Restarting k3s-agent after ${watch_iface} regained connectivity without flannel.1"
  systemctl restart k3s-agent
) >/dev/null 2>&1 &
SCRIPT_EOF

chmod 0755 /etc/NetworkManager/dispatcher.d/90-k3s-agent-recover

cat > /etc/default/k3s-agent-recover <<EOF
FLANNEL_IFACE="${FLANNEL_IFACE}"
RECOVERY_WAIT_SECONDS="${RECOVERY_WAIT_SECONDS}"
EOF

python3 - <<'PY'
from pathlib import Path

dispatcher = Path("/etc/NetworkManager/dispatcher.d/90-k3s-agent-recover")
defaults = Path("/etc/default/k3s-agent-recover")
text = dispatcher.read_text()
if 'source /etc/default/k3s-agent-recover' not in text:
    text = text.replace(
        'watch_iface="${FLANNEL_IFACE:-}"\nwait_seconds="${RECOVERY_WAIT_SECONDS:-8}"\n',
        'watch_iface="${FLANNEL_IFACE:-}"\nwait_seconds="${RECOVERY_WAIT_SECONDS:-8}"\nif [[ -f /etc/default/k3s-agent-recover ]]; then\n  # shellcheck disable=SC1091\n  source /etc/default/k3s-agent-recover\nfi\nwatch_iface="${FLANNEL_IFACE:-$watch_iface}"\nwait_seconds="${RECOVERY_WAIT_SECONDS:-$wait_seconds}"\n',
    )
    dispatcher.write_text(text)
PY
REMOTE_SCRIPT
done

echo "Installed k3s network recovery hook on configured worker nodes."
