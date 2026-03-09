#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="$ROOT_DIR/.env"
CLUSTER_CONFIG_FILE="$ROOT_DIR/config/cluster.env"

FOLLOW_SCRIPT_PATH="/tmp/atlasnet_node_logs_follow.sh"
PID_FILE_PATH="/tmp/atlasnet_node_logs.pid"
LOG_OUTPUT_PATH="/tmp/atlasnet-node-logs.out"
LAUNCH_LOG_PATH="/tmp/atlasnet-node-logs-launch.log"

usage() {
  cat <<USAGE
Usage: $(basename "$0") <start|stop>
USAGE
}

die() {
  echo "ERROR: $*" >&2
  exit 1
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."
}

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

ACTION="${1:-}"
[[ "$ACTION" == "start" || "$ACTION" == "stop" ]] || {
  usage
  exit 1
}

if [[ -f "$ENV_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$ENV_FILE"
fi
if [[ -f "$CLUSTER_CONFIG_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$CLUSTER_CONFIG_FILE"
fi

: "${SERVER_IPS:?Set SERVER_IPS in .env}"
: "${SERVER_SSH_USER:?Set SERVER_SSH_USER in .env}"
: "${WORKER_IPS:=}"
: "${WORKER_SSH_USER:=pi}"
: "${SSH_KEY:=$HOME/.ssh/id_ed25519}"

SSH_KEY="${SSH_KEY/#\~/$HOME}"
[[ -f "$SSH_KEY" ]] || die "SSH key file does not exist: $SSH_KEY"

need_cmd ssh

SSH_OPTS=(
  -i "$SSH_KEY"
  -o StrictHostKeyChecking=accept-new
  -o ConnectTimeout=8
)

declare -A TARGETS=()

add_targets() {
  local entries="$1"
  local default_user="$2"

  for entry in $entries; do
    [[ -n "$entry" ]] || continue

    local parsed user host key
    parsed="$(parse_entry "$entry" "$default_user")"
    user="${parsed%%|*}"
    host="${parsed##*|}"
    key="${user}@${host}"

    TARGETS["$key"]="$user|$host"
  done
}

add_targets "$SERVER_IPS" "$SERVER_SSH_USER"
add_targets "$WORKER_IPS" "$WORKER_SSH_USER"

run_on_target() {
  local user="$1"
  local host="$2"

  ssh "${SSH_OPTS[@]}" "${user}@${host}" bash -s -- "$ACTION" <<'REMOTE_SCRIPT'
set -euo pipefail

ACTION="$1"
FOLLOW_SCRIPT_PATH="/tmp/atlasnet_node_logs_follow.sh"
PID_FILE_PATH="/tmp/atlasnet_node_logs.pid"
LOG_OUTPUT_PATH="/tmp/atlasnet-node-logs.out"
LAUNCH_LOG_PATH="/tmp/atlasnet-node-logs-launch.log"

write_follow_script() {
  cat > "$FOLLOW_SCRIPT_PATH" <<'SCRIPT_EOF'
#!/usr/bin/env bash
set -euo pipefail
sudo -n find /var/log/pods -type f -name '*.log' -print0 | xargs -0 sudo -n tail -n 0 -F
SCRIPT_EOF
  chmod +x "$FOLLOW_SCRIPT_PATH"
}

is_running() {
  pgrep -f "$FOLLOW_SCRIPT_PATH" >/dev/null 2>&1
}

ensure_passwordless_sudo() {
  if sudo -n true >/dev/null 2>&1; then
    return 0
  fi

  echo "passwordless sudo is not configured for $(id -un) on $(hostname -s)"
  return 1
}

start_headless() {
  if is_running; then
    echo "already running"
    return 0
  fi

  nohup bash "$FOLLOW_SCRIPT_PATH" >"$LOG_OUTPUT_PATH" 2>&1 < /dev/null &
  echo "$!" > "$PID_FILE_PATH"
  sleep 1
  if is_running; then
    echo "started (headless)"
    return 0
  fi

  echo "failed to start (headless); recent output:"
  tail -n 20 "$LOG_OUTPUT_PATH" 2>/dev/null || true
  return 1
}

start_desktop_terminal() {
  command -v x-terminal-emulator >/dev/null 2>&1 || return 1

  local display xdg_runtime_dir
  display="${DISPLAY:-:0}"
  xdg_runtime_dir="/run/user/$(id -u)"

  nohup env \
    DISPLAY="$display" \
    XDG_RUNTIME_DIR="$xdg_runtime_dir" \
    x-terminal-emulator -e bash -lc "$FOLLOW_SCRIPT_PATH; exec bash" \
    >"$LAUNCH_LOG_PATH" 2>&1 < /dev/null &

  sleep 1
  if is_running; then
    echo "started (desktop terminal)"
    return 0
  fi

  echo "desktop launch attempted but follower not running; launch log:"
  tail -n 20 "$LAUNCH_LOG_PATH" 2>/dev/null || true
  return 1
}

stop_all() {
  if [[ -f "$PID_FILE_PATH" ]]; then
    kill "$(cat "$PID_FILE_PATH")" >/dev/null 2>&1 || true
    rm -f "$PID_FILE_PATH"
  fi

  pkill -f "$FOLLOW_SCRIPT_PATH" >/dev/null 2>&1 || true
  echo "stopped"
}

write_follow_script

case "$ACTION" in
  start)
    ensure_passwordless_sudo
    if start_desktop_terminal; then
      exit 0
    fi
    start_headless
    ;;
  stop)
    stop_all
    ;;
  *)
    echo "invalid action: $ACTION" >&2
    exit 1
    ;;
esac
REMOTE_SCRIPT
}

failed=0

for key in $(printf '%s\n' "${!TARGETS[@]}" | sort); do
  user_host="${TARGETS[$key]}"
  user="${user_host%%|*}"
  host="${user_host##*|}"

  echo "[$ACTION] ${user}@${host}"
  if ! run_on_target "$user" "$host"; then
    echo "[$ACTION] FAILED: ${user}@${host}" >&2
    failed=1
  fi
done

exit "$failed"
