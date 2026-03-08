#!/usr/bin/env bash
set -euo pipefail

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install openssh-client."; }

# At least one server required
: "${SERVER_IPS:?Set SERVER_IPS in .env}"
: "${SERVER_SSH_USER:?Set SERVER_SSH_USER in .env}"
: "${WORKER_SSH_USER:=pi}"
: "${SSH_KEY:=$HOME/.ssh/id_ed25519}"
: "${WORKER_IPS:=}"

WORKERS="$(echo "$WORKER_IPS" | xargs)"

SSH_KEY="${SSH_KEY/#\~/$HOME}"
KEY_DIR="$(dirname "$SSH_KEY")"

need_cmd ssh
need_cmd ssh-keygen
need_cmd ssh-copy-id

mkdir -p "$KEY_DIR"
if [[ -f "$SSH_KEY" ]]; then
  echo "SSH key already exists: $SSH_KEY"
else
  echo "Creating SSH key: $SSH_KEY"
  ssh-keygen -t ed25519 -a 100 -f "$SSH_KEY" -N "" -C "k3s-homelab"
fi

ensure_key_access() {
  local user="$1"
  local ip="$2"
  local label="$3"

  echo "Ensuring key access to ${label}: ${user}@${ip}"
  if ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${user}@${ip}" true >/dev/null 2>&1; then
    echo " - ${label} key auth already works"
  else
    ssh-copy-id -i "${SSH_KEY}.pub" -o StrictHostKeyChecking=accept-new "${user}@${ip}"
  fi
}

for entry in $SERVER_IPS; do
  [[ -n "$entry" ]] || continue
  # Strip any stray double quotes that might sneak in from .env formatting
  entry="${entry//\"/}"
  if [[ "$entry" == *@* ]]; then
    user="${entry%@*}"
    host="${entry#*@}"
  else
    user="$SERVER_SSH_USER"
    host="$entry"
  fi
  ensure_key_access "$user" "$host" "server ${host}"
done
for entry in $WORKERS; do
  [[ -n "$entry" ]] || continue
  entry="${entry//\"/}"
  if [[ "$entry" == *@* ]]; then
    user="${entry%@*}"
    host="${entry#*@}"
  else
    user="$WORKER_SSH_USER"
    host="$entry"
  fi
  ensure_key_access "$user" "$host" "worker ${host}"
done

echo "SSH setup done. Run 'make k3s-deploy'."
