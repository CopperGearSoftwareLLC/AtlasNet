#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
source_env

: "${SERVER_IPS:?Set SERVER_IPS in .env}"
: "${SERVER_SSH_USER:?Set SERVER_SSH_USER in .env}"
: "${WORKER_SSH_USER:=pi}"
: "${WORKER_IPS:=}"

need_cmd ssh
need_cmd ssh-keygen
need_cmd ssh-keyscan
need_cmd ssh-copy-id

SSH_KEY="$(ssh_key_path)"
KEY_DIR="$(dirname "$SSH_KEY")"

mkdir -p "$KEY_DIR"
if [[ -f "$SSH_KEY" ]]; then
  echo "SSH key already exists: $SSH_KEY"
else
  echo "Creating SSH key: $SSH_KEY"
  ssh-keygen -t ed25519 -a 100 -f "$SSH_KEY" -N "" -C "atlasnet-k3s-offline"
fi

ensure_key_access() {
  local role="$1"
  local user="$2"
  local host="$3"
  local ssh_output

  echo "Ensuring key access to ${role} ${host}: ${user}@${host}"
  ssh_output="$(
    ssh -i "$SSH_KEY" \
      -o BatchMode=yes \
      -o StrictHostKeyChecking=accept-new \
      -o ConnectTimeout=5 \
      "${user}@${host}" true 2>&1
  )" && {
    echo " - key auth already works"
    return 0
  }

  if grep -q "REMOTE HOST IDENTIFICATION HAS CHANGED" <<<"$ssh_output"; then
    echo " - refreshing changed host key for ${host}"
    ssh-keygen -R "$host" >/dev/null
    ssh-keygen -R "[$host]:22" >/dev/null 2>&1 || true
    ssh-keyscan -H "$host" >> "$HOME/.ssh/known_hosts" 2>/dev/null || true
  fi

  ssh-copy-id -i "${SSH_KEY}.pub" -o StrictHostKeyChecking=accept-new "${user}@${host}"
}

for_each_node ensure_key_access

echo "SSH setup done."
