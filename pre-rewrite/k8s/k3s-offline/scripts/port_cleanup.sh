#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
source_env
require_ssh_key

: "${SERVER_PORT_CLEANUP_PORTS:=7946}"
: "${WORKER_PORT_CLEANUP_PORTS:=7946}"

need_cmd ssh
SSH_KEY="$(ssh_key_path)"

append_port_if_missing() {
  local list="$1"
  local want="$2"
  local item

  for item in $list; do
    [[ "$item" == "$want" ]] && {
      echo "$list"
      return 0
    }
  done

  echo "$list $want"
}

SERVER_PORT_CLEANUP_PORTS="$(append_port_if_missing "$SERVER_PORT_CLEANUP_PORTS" "6443")"
SERVER_PORT_CLEANUP_PORTS="$(echo "$SERVER_PORT_CLEANUP_PORTS" | xargs)"
WORKER_PORT_CLEANUP_PORTS="$(echo "$WORKER_PORT_CLEANUP_PORTS" | xargs)"

cleanup_remote_ports() {
  local role="$1"
  local user="$2"
  local host="$3"
  local ports="$4"

  [[ -n "${ports// }" ]] || {
    echo "${role} ${host}: no cleanup ports configured"
    return 0
  }

  echo "${role} ${host}: cleaning ports [${ports}]"
  ssh -T -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${user}@${host}" \
    "PORTS='${ports}' bash -s" <<'REMOTE'
set -euo pipefail

if ! sudo -n true >/dev/null 2>&1; then
  echo "ERROR: passwordless sudo required for port cleanup"
  exit 1
fi

get_pids_for_port() {
  local port="$1"
  sudo ss -lntupH "( sport = :${port} )" 2>/dev/null \
    | grep -oE 'pid=[0-9]+' \
    | cut -d= -f2 \
    | sort -u
}

get_comm() {
  local pid="$1"
  ps -p "$pid" -o comm= 2>/dev/null || true
}

get_service_unit_from_pid() {
  local pid="$1"
  [[ -r "/proc/${pid}/cgroup" ]] || return 1
  awk -F/ '
    {
      for (i = 1; i <= NF; i++) {
        if ($i ~ /\.service$/) {
          print $i
          exit
        }
      }
    }
  ' "/proc/${pid}/cgroup"
}

kill_pid_if_needed() {
  local pid="$1"
  kill -0 "$pid" >/dev/null 2>&1 || return 0
  sudo kill -TERM "$pid" >/dev/null 2>&1 || true
  sleep 1
  kill -0 "$pid" >/dev/null 2>&1 && sudo kill -KILL "$pid" >/dev/null 2>&1 || true
}

for port in $PORTS; do
  pids="$(get_pids_for_port "$port" || true)"
  if [[ -z "${pids// }" ]]; then
    echo " - port ${port}: already free"
    continue
  fi

  echo " - port ${port}: listeners detected"
  for pid in $pids; do
    comm="$(get_comm "$pid")"

    if [[ "$port" == "6443" && "$comm" =~ ^k3s(|-server)$ ]]; then
      echo "   - keeping k3s listener on 6443"
      continue
    fi

    unit="$(get_service_unit_from_pid "$pid" || true)"
    if [[ -n "$unit" ]]; then
      echo "   - stopping service ${unit}"
      sudo systemctl stop "$unit" >/dev/null 2>&1 || true
    fi

    if kill -0 "$pid" >/dev/null 2>&1; then
      echo "   - terminating pid ${pid} (${comm:-unknown})"
      kill_pid_if_needed "$pid"
    fi
  done

  remaining="$(sudo ss -lntupH "( sport = :${port} )" 2>/dev/null || true)"
  if [[ "$port" == "6443" && -n "${remaining// }" ]]; then
    remaining_non_k3s="$(printf '%s\n' "$remaining" | awk 'NF && $0 !~ /k3s/' || true)"
    [[ -z "${remaining_non_k3s// }" ]] && continue
  fi
  [[ -z "${remaining// }" ]] || {
    echo "ERROR: port ${port} still in use:"
    echo "$remaining"
    exit 1
  }

  echo " - port ${port}: now free"
done
REMOTE
}

cleanup_server() {
  cleanup_remote_ports "$1" "$2" "$3" "$SERVER_PORT_CLEANUP_PORTS"
}

cleanup_worker() {
  cleanup_remote_ports "$1" "$2" "$3" "$WORKER_PORT_CLEANUP_PORTS"
}

for entry in $SERVER_IPS; do
  [[ -n "$entry" ]] || continue
  read -r user host <<<"$(parse_node_entry "$entry" "$SERVER_SSH_USER")"
  cleanup_server "server" "$user" "$host"
done
for entry in ${WORKER_IPS:-}; do
  [[ -n "$entry" ]] || continue
  read -r user host <<<"$(parse_node_entry "$entry" "$WORKER_SSH_USER")"
  cleanup_worker "worker" "$user" "$host"
done

echo "Port cleanup done."
