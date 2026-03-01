#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="$ROOT_DIR/.env"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }
normalize_ports() { printf '%s' "${1//,/ }"; }
normalize_hosts() { printf '%s' "${1//,/ }"; }
has_port() {
  local ports="$1"
  local needle="$2"
  local p
  for p in $ports; do
    [[ "$p" == "$needle" ]] && return 0
  done
  return 1
}
validate_ports() {
  local ports="$1"
  local p
  for p in $ports; do
    [[ "$p" =~ ^[0-9]+$ ]] || die "Invalid port '${p}'. Use numeric ports only."
    ((p >= 1 && p <= 65535)) || die "Port '${p}' out of range (1-65535)."
  done
}
is_local_target() {
  local host_user="$1"
  local host_ip="$2"
  local ip

  [[ "$host_user" == "$USER" ]] || return 1
  [[ "$host_ip" == "127.0.0.1" || "$host_ip" == "localhost" ]] && return 0

  for ip in $(hostname -I 2>/dev/null || true); do
    [[ "$ip" == "$host_ip" ]] && return 0
  done

  if command -v ip >/dev/null 2>&1; then
    while read -r ip; do
      [[ "$ip" == "$host_ip" ]] && return 0
    done < <(ip -o -4 addr show scope global 2>/dev/null | awk '{split($4, a, "/"); print a[1]}')
  fi
  return 1
}
cleanup_payload() {
  cat <<'PAYLOAD'
set -euo pipefail

if ! sudo -n true >/dev/null 2>&1; then
  echo "ERROR: passwordless sudo required for port cleanup"
  exit 1
fi
if ! command -v ss >/dev/null 2>&1; then
  echo "ERROR: 'ss' is required on remote host for port cleanup"
  exit 1
fi
: "${STOP_K3S_API_LISTENER:=false}"

has_fuser=0
if command -v fuser >/dev/null 2>&1; then
  has_fuser=1
fi

listeners_for_port() {
  local port="$1"
  sudo ss -H -lntup "( sport = :${port} )" 2>/dev/null || true
}

port_has_listener() {
  local port="$1"
  [[ -n "$(listeners_for_port "$port")" ]]
}

get_pids_for_port() {
  local port="$1"
  listeners_for_port "$port" \
    | grep -oE 'pid=[0-9]+' \
    | cut -d= -f2 \
    | sort -u \
    || true
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
  if ! [[ "$port" =~ ^[0-9]+$ ]] || ((port < 1 || port > 65535)); then
    echo "ERROR: invalid port value '${port}'"
    exit 1
  fi

  if ! port_has_listener "$port"; then
    echo " - port ${port}: already free"
    continue
  fi

  pids="$(get_pids_for_port "$port")"
  if [[ -z "${pids// }" ]]; then
    echo " - port ${port}: listener detected, but process ID is not visible"
    if ((has_fuser == 1)); then
      echo "   - using fuser fallback on tcp/udp"
      sudo fuser -k -n tcp "$port" >/dev/null 2>&1 || true
      sudo fuser -k -n udp "$port" >/dev/null 2>&1 || true
    fi
  else
    echo " - port ${port}: listeners detected"
    preserved_k3s_api=0
    for pid in $pids; do
      comm="$(get_comm "$pid")"
      unit="$(get_service_unit_from_pid "$pid" || true)"

      if [[ "$port" == "6443" && "$STOP_K3S_API_LISTENER" != "true" ]]; then
        if [[ "$comm" == k3s* || "$unit" == "k3s.service" ]]; then
          echo "   - preserving existing k3s listener on 6443 (set SERVER_STOP_K3S_ON_PORT_CLEANUP=true to stop it)"
          preserved_k3s_api=1
          continue
        fi
      fi

      # Docker Swarm uses 7946 and can block MetalLB speaker startup.
      if [[ "$port" == "7946" && "$comm" == "dockerd" ]] && command -v docker >/dev/null 2>&1; then
        swarm_state="$(sudo docker info --format '{{.Swarm.LocalNodeState}}' 2>/dev/null || true)"
        if [[ "$swarm_state" == "active" || "$swarm_state" == "pending" ]]; then
          echo "   - dockerd swarm detected; leaving swarm to free 7946"
          sudo docker swarm leave --force >/dev/null 2>&1 || true
          sleep 1
          continue
        fi
      fi

      if [[ -n "$unit" ]]; then
        echo "   - stopping service ${unit} (pid ${pid}, comm ${comm:-unknown})"
        sudo systemctl stop "$unit" >/dev/null 2>&1 || true
      fi

      if kill -0 "$pid" >/dev/null 2>&1; then
        echo "   - terminating pid ${pid} (${comm:-unknown})"
        kill_pid_if_needed "$pid"
      fi
    done
    if ((preserved_k3s_api == 1)) && port_has_listener "$port"; then
      echo " - port ${port}: kept running for existing k3s API listener"
      continue
    fi
  fi

  for _ in 1 2 3 4 5; do
    if ! port_has_listener "$port"; then
      break
    fi
    sleep 1
  done

  remaining="$(listeners_for_port "$port")"
  if [[ -n "${remaining// }" ]]; then
    echo "ERROR: port ${port} still in use:"
    echo "$remaining"
    exit 1
  fi

  echo " - port ${port}: now free"
done
PAYLOAD
}

[[ -f "$ENV_FILE" ]] || die "Missing .env at $ENV_FILE"
# shellcheck disable=SC1090
source "$ENV_FILE"

: "${SERVER_IP:?Set SERVER_IP in .env}"
: "${SERVER_SSH_USER:?Set SERVER_SSH_USER in .env}"
: "${WORKER_SSH_USER:=pi}"
: "${SSH_KEY:=$HOME/.ssh/id_ed25519}"
: "${WORKER_IPS:=${PI_WORKER_IP:-}}"
: "${SERVER_PORT_CLEANUP_PORTS:=7946}"
: "${WORKER_PORT_CLEANUP_PORTS:=7946}"
: "${SERVER_CLEAN_K3S_API_PORT:=true}"
: "${SERVER_STOP_K3S_ON_PORT_CLEANUP:=false}"

SSH_KEY="${SSH_KEY/#\~/$HOME}"
[[ -f "$SSH_KEY" ]] || die "SSH key file does not exist: $SSH_KEY"
WORKER_IPS="$(normalize_hosts "$WORKER_IPS")"

need_cmd ssh

SERVER_PORT_CLEANUP_PORTS="$(normalize_ports "$SERVER_PORT_CLEANUP_PORTS")"
WORKER_PORT_CLEANUP_PORTS="$(normalize_ports "$WORKER_PORT_CLEANUP_PORTS")"
if [[ "$SERVER_CLEAN_K3S_API_PORT" == "true" ]] && ! has_port "$SERVER_PORT_CLEANUP_PORTS" "6443"; then
  SERVER_PORT_CLEANUP_PORTS="${SERVER_PORT_CLEANUP_PORTS} 6443"
fi
validate_ports "$SERVER_PORT_CLEANUP_PORTS"
validate_ports "$WORKER_PORT_CLEANUP_PORTS"

cleanup_ports() {
  local host_user="$1"
  local host_ip="$2"
  local host_label="$3"
  local ports="$4"
  local stop_k3s_listener="$5"
  local remote_cmd

  [[ -n "${ports// }" ]] || {
    echo "${host_label}: no cleanup ports configured, skipping"
    return 0
  }

  if is_local_target "$host_user" "$host_ip"; then
    echo "${host_label}: cleaning ports [${ports}] on local host (${host_user}@${host_ip})"
    cleanup_payload | PORTS="$ports" STOP_K3S_API_LISTENER="$stop_k3s_listener" bash -seuo pipefail
    return
  fi

  echo "${host_label}: cleaning ports [${ports}] on ${host_user}@${host_ip}"
  printf -v remote_cmd 'PORTS=%q STOP_K3S_API_LISTENER=%q bash -seuo pipefail -s' "$ports" "$stop_k3s_listener"
  cleanup_payload | ssh -T -i "$SSH_KEY" \
    -o BatchMode=yes \
    -o StrictHostKeyChecking=accept-new \
    -o ConnectTimeout=5 \
    -o ServerAliveInterval=5 \
    -o ServerAliveCountMax=3 \
    "${host_user}@${host_ip}" \
    "$remote_cmd"
}

cleanup_ports "$SERVER_SSH_USER" "$SERVER_IP" "server" "$SERVER_PORT_CLEANUP_PORTS" "$SERVER_STOP_K3S_ON_PORT_CLEANUP"
if [[ -z "${WORKER_IPS// }" ]]; then
  echo "No workers configured (WORKER_IPS/PI_WORKER_IP is empty); skipping worker port cleanup."
else
  for ip in $WORKER_IPS; do
    cleanup_ports "$WORKER_SSH_USER" "$ip" "worker ${ip}" "$WORKER_PORT_CLEANUP_PORTS" "false"
  done
fi

echo "Port cleanup done."
