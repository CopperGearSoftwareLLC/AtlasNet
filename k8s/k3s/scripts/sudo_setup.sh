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
[[ -f "$SSH_KEY" ]] || die "SSH key file does not exist: $SSH_KEY"

need_cmd ssh

check_ssh_access() {
  local host_ip="$1"
  local host_user="$2"

  if ! ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${host_user}@${host_ip}" true >/dev/null 2>&1; then
    die "SSH auth failed for ${host_user}@${host_ip}. Run 'make ssh-setup' first."
  fi
}

ensure_passwordless_sudo() {
  local host_ip="$1"
  local host_user="$2"
  local label="$3"
  local sudoers_file
  local sudoers_line

  if [[ "$host_user" == "root" ]]; then
    echo "Skipping ${label}: root SSH user does not need sudo setup."
    return 0
  fi

  [[ "$host_user" =~ ^[A-Za-z_][A-Za-z0-9_-]*$ ]] || \
    die "Unsupported SSH username for sudo automation: $host_user"

  if ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${host_user}@${host_ip}" 'sudo -n true' >/dev/null 2>&1; then
    echo "${label}: passwordless sudo already enabled."
    return 0
  fi

  sudoers_file="/etc/sudoers.d/k3sup-${host_user}"
  sudoers_line="${host_user} ALL=(ALL) NOPASSWD:ALL"

  echo "${label}: enabling passwordless sudo for ${host_user} (may prompt for sudo password)..."
  ssh -tt -i "$SSH_KEY" \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${host_user}@${host_ip}" \
    "set -e; echo '${sudoers_line}' | sudo tee '${sudoers_file}' >/dev/null; sudo chmod 440 '${sudoers_file}'; sudo visudo -cf '${sudoers_file}' >/dev/null"

  if ! ssh -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${host_user}@${host_ip}" 'sudo -n true' >/dev/null 2>&1; then
    die "${label}: passwordless sudo setup failed for ${host_user}@${host_ip}"
  fi
}

echo "Checking SSH access before sudo setup ..."
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
  check_ssh_access "$host" "$user"
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
  check_ssh_access "$host" "$user"
done

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
  ensure_passwordless_sudo "$host" "$user" "server ${host}"
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
  ensure_passwordless_sudo "$host" "$user" "worker ${host}"
done

echo "Sudo setup done. Run 'make k3s-deploy'."
