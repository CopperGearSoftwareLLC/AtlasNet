#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="$ROOT_DIR/.env"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

[[ -f "$ENV_FILE" ]] || die "Missing .env at $ENV_FILE"
# shellcheck disable=SC1090
source "$ENV_FILE"

# At least one server required
: "${SERVER_IPS:?Set SERVER_IPS in .env}"
: "${SERVER_SSH_USER:?Set SERVER_SSH_USER in .env}"
: "${WORKER_SSH_USER:=pi}"
: "${SSH_KEY:=$HOME/.ssh/id_ed25519}"
: "${SERVER_PORT_CLEANUP_PORTS:=7946}"
: "${WORKER_PORT_CLEANUP_PORTS:=7946}"
: "${WORKER_IPS:=}"

WORKERS="$(echo "$WORKER_IPS" | xargs)"

append_port_if_missing() {
  local list="$1"
  local want="$2"
  local item

  for item in $list; do
    if [[ "$item" == "$want" ]]; then
      echo "$list"
      return 0
    fi
  done

  echo "$list $want"
}

# Always preflight server API port conflicts for k3s.
SERVER_PORT_CLEANUP_PORTS="$(append_port_if_missing "$SERVER_PORT_CLEANUP_PORTS" "6443")"
SERVER_PORT_CLEANUP_PORTS="$(echo "$SERVER_PORT_CLEANUP_PORTS" | xargs)"

SSH_KEY="${SSH_KEY/#\~/$HOME}"
[[ -f "$SSH_KEY" ]] || die "SSH key file does not exist: $SSH_KEY"

need_cmd ssh

cleanup_remote_ports() {
  local host_user="$1"
  local host_ip="$2"
  local host_label="$3"
  local ports="$4"

  [[ -n "${ports// }" ]] || {
    echo "${host_label}: no cleanup ports configured, skipping"
    return 0
  }

  echo "${host_label}: cleaning ports [${ports}] on ${host_user}@${host_ip}"
  ssh -T -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    "${host_user}@${host_ip}" \
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
  if ! kill -0 "$pid" >/dev/null 2>&1; then
    return 0
  fi

  sudo kill -TERM "$pid" >/dev/null 2>&1 || true
  sleep 1

  if kill -0 "$pid" >/dev/null 2>&1; then
    sudo kill -KILL "$pid" >/dev/null 2>&1 || true
  fi
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

    # Keep k3s on the API port when it's already healthy on reruns.
    if [[ "$port" == "6443" && "$comm" =~ ^k3s(|-server)$ ]]; then
      echo "   - keeping k3s listener on 6443 (pid ${pid})"
      continue
    fi

    # Docker Swarm uses 7946 and blocks MetalLB speaker.
    if [[ "$port" == "7946" && "$comm" == "dockerd" ]]; then
      swarm_state="$(sudo docker info --format '{{.Swarm.LocalNodeState}}' 2>/dev/null || true)"
      if [[ "$swarm_state" == "active" || "$swarm_state" == "pending" ]]; then
        echo "   - dockerd swarm detected; leaving swarm to free 7946"
        sudo docker swarm leave --force >/dev/null 2>&1 || true
        sleep 1
        continue
      fi
    fi

    unit="$(get_service_unit_from_pid "$pid" || true)"
    if [[ -n "$unit" ]]; then
      echo "   - stopping service ${unit} (pid ${pid}, comm ${comm:-unknown})"
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
    if [[ -z "${remaining_non_k3s// }" ]]; then
      echo " - port 6443: reserved by k3s (allowed)"
      continue
    fi
  fi
  if [[ -n "${remaining// }" ]]; then
    echo "ERROR: port ${port} still in use:"
    echo "$remaining"
    exit 1
  fi

  echo " - port ${port}: now free"
done
REMOTE
}

for entry in $SERVER_IPS; do
  [[ -n "$entry" ]] || continue
  if [[ "$entry" == *@* ]]; then
    user="${entry%@*}"
    host="${entry#*@}"
  else
    user="$SERVER_SSH_USER"
    host="$entry"
  fi
  cleanup_remote_ports "$user" "$host" "server ${host}" "$SERVER_PORT_CLEANUP_PORTS"
done
for entry in $WORKERS; do
  [[ -n "$entry" ]] || continue
  if [[ "$entry" == *@* ]]; then
    user="${entry%@*}"
    host="${entry#*@}"
  else
    user="$WORKER_SSH_USER"
    host="$entry"
  fi
  cleanup_remote_ports "$user" "$host" "worker ${host}" "$WORKER_PORT_CLEANUP_PORTS"
done

echo "Port cleanup done."
