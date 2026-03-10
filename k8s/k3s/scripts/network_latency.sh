#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="$ROOT_DIR/.env"
CLUSTER_CONFIG_FILE="$ROOT_DIR/config/cluster.env"

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
CLI_TARGETS="${2:-}"
case "$ACTION" in
  apply|apply-auto-worker|clear-all)
    ;;
  *)
    cat <<USAGE >&2
Usage:
  $(basename "$0") apply "<targets>"
  $(basename "$0") apply-auto-worker
  $(basename "$0") clear-all
USAGE
    exit 1
    ;;
esac

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
: "${LATENCY_DELAY:=100ms}"
: "${LATENCY_JITTER:=20ms}"

SSH_KEY="${SSH_KEY/#\~/$HOME}"
[[ -f "$SSH_KEY" ]] || die "SSH key file does not exist: $SSH_KEY"

need_cmd ssh

SSH_OPTS=(
  -i "$SSH_KEY"
  -o StrictHostKeyChecking=accept-new
  -o ConnectTimeout=8
)

declare -A TARGETS=()
declare -A INVENTORY_BY_HOST=()
declare -A WORKER_BY_KEY=()
declare -a WORKER_KEYS=()

add_inventory_entries() {
  local entries="$1"
  local default_user="$2"
  local entry parsed user host

  for entry in $entries; do
    [[ -n "$entry" ]] || continue
    parsed="$(parse_entry "$entry" "$default_user")"
    user="${parsed%%|*}"
    host="${parsed##*|}"
    [[ -n "$host" ]] || continue

    if [[ -z "${INVENTORY_BY_HOST[$host]+x}" ]]; then
      INVENTORY_BY_HOST["$host"]="$user"
    fi
  done
}

add_target_token() {
  local token="$1"
  local user host key

  token="${token//\"/}"
  [[ -n "$token" ]] || return 0

  if [[ "$token" == *@* ]]; then
    user="${token%@*}"
    host="${token#*@}"
  else
    host="$token"
    user="${INVENTORY_BY_HOST[$host]:-${LATENCY_SSH_USER:-$SERVER_SSH_USER}}"
  fi

  [[ -n "$host" ]] || die "Invalid target: '$token'"
  key="${user}@${host}"
  TARGETS["$key"]="$user|$host"
}

add_targets_from_entries() {
  local entries="$1"
  local default_user="$2"
  local entry parsed user host key

  for entry in $entries; do
    [[ -n "$entry" ]] || continue
    parsed="$(parse_entry "$entry" "$default_user")"
    user="${parsed%%|*}"
    host="${parsed##*|}"
    key="${user}@${host}"
    TARGETS["$key"]="$user|$host"
  done
}

build_worker_list() {
  local entry parsed user host key

  WORKER_KEYS=()
  WORKER_BY_KEY=()

  for entry in $WORKER_IPS; do
    [[ -n "$entry" ]] || continue
    parsed="$(parse_entry "$entry" "$WORKER_SSH_USER")"
    user="${parsed%%|*}"
    host="${parsed##*|}"
    key="${user}@${host}"
    if [[ -z "${WORKER_BY_KEY[$key]+x}" ]]; then
      WORKER_KEYS+=("$key")
      WORKER_BY_KEY["$key"]="$user|$host"
    fi
  done
}

run_on_target() {
  local action="$1"
  local user="$2"
  local host="$3"

  ssh "${SSH_OPTS[@]}" "${user}@${host}" bash -s -- "$action" "$LATENCY_DELAY" "$LATENCY_JITTER" <<'REMOTE_SCRIPT'
set -euo pipefail

ACTION="$1"
LATENCY_DELAY="$2"
LATENCY_JITTER="$3"

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || {
    echo "missing command: $1" >&2
    exit 1
  }
}

need_cmd ip
need_cmd tc
need_cmd sudo

if ! sudo -n true >/dev/null 2>&1; then
  echo "passwordless sudo is required on this node" >&2
  exit 1
fi

IFACE="$(ip route show default | awk '{print $5}' | head -n 1)"
[[ -n "$IFACE" ]] || {
  echo "failed to determine default network interface" >&2
  exit 1
}

case "$ACTION" in
  apply)
    sudo -n tc qdisc replace dev "$IFACE" root netem delay "$LATENCY_DELAY" "$LATENCY_JITTER"
    ;;
  clear-all)
    sudo -n tc qdisc del dev "$IFACE" root >/dev/null 2>&1 || true
    ;;
  *)
    echo "invalid action: $ACTION" >&2
    exit 1
    ;;
esac
REMOTE_SCRIPT
}

add_inventory_entries "$SERVER_IPS" "$SERVER_SSH_USER"
add_inventory_entries "$WORKER_IPS" "$WORKER_SSH_USER"

failed=0

if [[ "$ACTION" == "apply" ]]; then
  targets="${CLI_TARGETS:-${LATENCY_TARGETS:-}}"
  : "${targets:?Set LATENCY_TARGETS (e.g. LATENCY_TARGETS=\"ubuntu0@192.168.122.41 192.168.88.12\")}" 
  for token in $targets; do
    add_target_token "$token"
  done

  mapfile -t keys_sorted < <(printf '%s\n' "${!TARGETS[@]}" | sort)
  for key in "${keys_sorted[@]}"; do
    user_host="${TARGETS[$key]}"
    user="${user_host%%|*}"
    host="${user_host##*|}"

    if ! run_on_target "apply" "$user" "$host"; then
      echo "[apply] FAILED: ${user}@${host}" >&2
      failed=1
    else
      echo "latency applied: ${user}@${host}"
    fi
  done

elif [[ "$ACTION" == "apply-auto-worker" ]]; then
  build_worker_list
  worker_count="${#WORKER_KEYS[@]}"
  (( worker_count > 0 )) || die "WORKER_IPS is empty; cannot pick a random worker"

  selected_index="$(( RANDOM % worker_count ))"
  selected_key="${WORKER_KEYS[$selected_index]}"
  selected_info="${WORKER_BY_KEY[$selected_key]}"
  selected_user="${selected_info%%|*}"
  selected_host="${selected_info##*|}"

  if ! run_on_target "apply" "$selected_user" "$selected_host"; then
    echo "[apply] FAILED: ${selected_user}@${selected_host}" >&2
    failed=1
  else
    echo "latency applied: ${selected_user}@${selected_host}"
  fi

else
  add_targets_from_entries "$SERVER_IPS" "$SERVER_SSH_USER"
  add_targets_from_entries "$WORKER_IPS" "$WORKER_SSH_USER"

  mapfile -t keys_sorted < <(printf '%s\n' "${!TARGETS[@]}" | sort)
  for key in "${keys_sorted[@]}"; do
    user_host="${TARGETS[$key]}"
    user="${user_host%%|*}"
    host="${user_host##*|}"

    if ! run_on_target "clear-all" "$user" "$host"; then
      echo "[clear-all] FAILED: ${user}@${host}" >&2
      failed=1
    fi
  done
fi

exit "$failed"
