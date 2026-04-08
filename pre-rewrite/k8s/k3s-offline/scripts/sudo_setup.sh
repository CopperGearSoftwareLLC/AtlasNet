#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
source_env
require_ssh_key

need_cmd ssh

SSH_KEY="$(ssh_key_path)"

check_ssh_access() {
  local _role="$1"
  local user="$2"
  local host="$3"

  if ! ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${user}@${host}" true >/dev/null 2>&1; then
    die "SSH auth failed for ${user}@${host}. Run 'make ssh-setup' first."
  fi
}

ensure_passwordless_sudo() {
  local role="$1"
  local user="$2"
  local host="$3"
  local sudoers_file sudoers_line

  [[ "$user" == "root" ]] && {
    echo "Skipping ${role} ${host}: root SSH user does not need sudo setup."
    return 0
  }

  if ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${user}@${host}" 'sudo -n true' >/dev/null 2>&1; then
    echo "${role} ${host}: passwordless sudo already enabled."
    return 0
  fi

  sudoers_file="/etc/sudoers.d/atlasnet-k3s-offline-${user}"
  sudoers_line="${user} ALL=(ALL) NOPASSWD:ALL"

  echo "${role} ${host}: enabling passwordless sudo for ${user}"
  ssh -tt -i "$SSH_KEY" \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${user}@${host}" \
    "set -e; echo '${sudoers_line}' | sudo tee '${sudoers_file}' >/dev/null; sudo chmod 440 '${sudoers_file}'; sudo visudo -cf '${sudoers_file}' >/dev/null"

  ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${user}@${host}" 'sudo -n true' >/dev/null 2>&1 || \
    die "${role} ${host}: passwordless sudo setup failed"
}

for_each_node check_ssh_access
for_each_node ensure_passwordless_sudo

echo "Sudo setup done."
