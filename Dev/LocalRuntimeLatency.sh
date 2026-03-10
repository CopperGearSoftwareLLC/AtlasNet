#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENSURE_HELPER_SCRIPT="${SCRIPT_DIR}/EnsureLatencyHelper.sh"
HELPER_NAME="${ATLASNET_LATENCY_HELPER_NAME:-atlasnet_latency_helper}"

RUNTIME="${1:-}"
ACTION="${2:-}"

usage() {
  cat <<'EOF'
Usage:
  LocalRuntimeLatency.sh [auto|swarm|k3d] apply-auto
  LocalRuntimeLatency.sh [auto|swarm|k3d] clear-all

Aliases:
  LocalRuntimeLatency.sh swarm apply-auto-shard
  LocalRuntimeLatency.sh k3d apply-auto-worker

Environment:
  ATLASNET_LOCAL_LATENCY_RUNTIME  Runtime selector: auto, swarm, or k3d (default: auto)
  ATLASNET_SWARM_STACK_NAME       Swarm stack name prefix (default: atlasnet_dev)
  ATLASNET_K3D_CLUSTER_NAME       k3d cluster name (default: atlasnet-dev)
  ATLASNET_LATENCY_HELPER_NAME    Helper container name (default: atlasnet_latency_helper)
  LATENCY_DELAY                   delay value passed to tc netem (default: 100ms)
  LATENCY_JITTER                  optional jitter value passed to tc netem (default: none)
EOF
}

die() {
  echo "ERROR: $*" >&2
  exit 1
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "Missing required command: $1"
}

pick_random_target() {
  local -n ref_targets=$1
  local selected_index="$(( RANDOM % ${#ref_targets[@]} ))"
  printf '%s\n' "${ref_targets[$selected_index]}"
}

normalize_runtime_and_action() {
  if [[ "$RUNTIME" == "-h" || "$RUNTIME" == "--help" || -z "$RUNTIME" ]]; then
    usage
    exit 0
  fi

  case "$RUNTIME" in
    auto|swarm|k3d)
      ;;
    apply-auto|clear-all|apply-auto-shard|apply-auto-worker)
      ACTION="$RUNTIME"
      RUNTIME="${ATLASNET_LOCAL_LATENCY_RUNTIME:-auto}"
      ;;
    *)
      usage >&2
      die "Invalid runtime: $RUNTIME"
      ;;
  esac

  case "$ACTION" in
    -h|--help|"")
      usage
      exit 0
      ;;
    apply-auto-shard|apply-auto-worker)
      ACTION="apply-auto"
      ;;
    apply-auto|clear-all)
      ;;
    *)
      usage >&2
      die "Invalid action: $ACTION"
      ;;
  esac
}

resolve_k3d_worker_targets() {
  local cluster_name="$1"
  mapfile -t resolved_k3d_workers < <(
    docker ps --format '{{.Names}}' | grep -E "^k3d-${cluster_name}-agent-[0-9]+$" || true
  )
}

resolve_swarm_shard_targets() {
  local service_name="$1"
  mapfile -t resolved_swarm_shards < <(
    docker ps --format '{{.Names}}' --filter "label=com.docker.swarm.service.name=${service_name}"
  )
}

detect_runtime() {
  local swarm_service_name="$1"
  local cluster_name="$2"

  resolve_swarm_shard_targets "$swarm_service_name"
  resolve_k3d_worker_targets "$cluster_name"

  if ((${#resolved_swarm_shards[@]} > 0)) && ((${#resolved_k3d_workers[@]} == 0)); then
    printf 'swarm\n'
    return
  fi

  if ((${#resolved_k3d_workers[@]} > 0)) && ((${#resolved_swarm_shards[@]} == 0)); then
    printf 'k3d\n'
    return
  fi

  if ((${#resolved_swarm_shards[@]} == 0)) && ((${#resolved_k3d_workers[@]} == 0)); then
    die "No running local runtime targets found. Start either the Swarm shard service or k3d worker nodes first."
  fi

  die "Both Swarm shard containers and k3d worker containers are running. Set ATLASNET_LOCAL_LATENCY_RUNTIME=swarm or k3d."
}

ensure_latency_helper() {
  [[ -x "$ENSURE_HELPER_SCRIPT" ]] || die "Missing helper setup script: $ENSURE_HELPER_SCRIPT"
  bash "$ENSURE_HELPER_SCRIPT"
}

helper_apply_latency() {
  local runtime_name="$1"
  local target_name="$2"

  docker exec "$HELPER_NAME" sh -eu -c '
    target_name="$1"
    latency_delay="$2"
    latency_jitter="$3"
    runtime_name="$4"

    pid="$(docker inspect -f "{{.State.Pid}}" "$target_name" 2>/dev/null || true)"
    case "$pid" in
      ""|0) echo "failed to resolve PID for ${target_name}" >&2; exit 1 ;;
    esac

    ifaces="$(nsenter -t "$pid" -n ip -o link show up 2>/dev/null | awk -F": " '"'"'$2 != "lo" {print $2}'"'"' | sed "s/@.*//" | sort -u)"
    [ -n "$ifaces" ] || { echo "failed to determine interfaces for ${target_name}" >&2; exit 1; }

    for iface in $ifaces; do
      if [ -n "$latency_jitter" ]; then
        nsenter -t "$pid" -n tc qdisc replace dev "$iface" root netem delay "$latency_delay" "$latency_jitter"
      else
        nsenter -t "$pid" -n tc qdisc replace dev "$iface" root netem delay "$latency_delay"
      fi
    done

    qdisc_output="$(for iface in $ifaces; do nsenter -t "$pid" -n tc qdisc show dev "$iface"; done 2>&1)"
    echo "$qdisc_output" | grep -q "netem" || {
      echo "latency verification failed: runtime=${runtime_name} target=${target_name} ifaces=${ifaces}" >&2
      exit 1
    }
    echo "latency applied: target=${target_name} delay=${latency_delay}"
  ' sh "$target_name" "$LATENCY_DELAY" "$LATENCY_JITTER" "$runtime_name"
}

helper_clear_latency() {
  local runtime_name="$1"
  shift
  local -a targets=("$@")
  local target_name

  if ((${#targets[@]} == 0)); then
    echo "No running ${runtime_name} targets found. Nothing to clear."
    return
  fi

  for target_name in "${targets[@]}"; do
    docker exec "$HELPER_NAME" sh -eu -c '
      target_name="$1"
      runtime_name="$2"

      pid="$(docker inspect -f "{{.State.Pid}}" "$target_name" 2>/dev/null || true)"
      case "$pid" in
        ""|0) echo "latency clear skipped: runtime=${runtime_name} target=${target_name} (container missing)" ; exit 0 ;;
      esac

      ifaces="$(nsenter -t "$pid" -n sh -eu -c '"'"'
        if command -v ip >/dev/null 2>&1; then
          ip -o link show 2>/dev/null | awk -F": " '"'"'"'"'"'"'"'"'$2 != "lo" {print $2}'"'"'"'"'"'"'"'"' | sed "s/@.*//" | sort -u
        elif [ -d /sys/class/net ]; then
          find /sys/class/net -mindepth 1 -maxdepth 1 -printf "%f\n" 2>/dev/null | grep -v "^lo$" | sort -u
        fi
      '"'"' 2>/dev/null || true)"
      [ -n "$ifaces" ] || { echo "latency clear skipped: runtime=${runtime_name} target=${target_name} (iface missing)" ; exit 0; }

      for iface in $ifaces; do
        nsenter -t "$pid" -n tc qdisc del dev "$iface" root >/dev/null 2>&1 || true
        nsenter -t "$pid" -n tc qdisc del dev "$iface" ingress >/dev/null 2>&1 || true
      done

      ifb_ifaces="$(printf "%s\n" "$ifaces" | grep -E "^ifb[0-9]+$" || true)"
      for iface in $ifb_ifaces; do
        nsenter -t "$pid" -n ip link set dev "$iface" down >/dev/null 2>&1 || true
        nsenter -t "$pid" -n ip link del "$iface" >/dev/null 2>&1 || true
      done

      qdisc_output="$(nsenter -t "$pid" -n tc qdisc show 2>&1 || true)"
      echo "$qdisc_output" | grep -q "netem" && {
        echo "latency clear verification failed: runtime=${runtime_name} target=${target_name} ifaces=${ifaces}" >&2
        exit 1
      }
      echo "latency cleared: target=${target_name}"
    ' sh "$target_name" "$runtime_name"
  done
}

normalize_runtime_and_action

need_cmd docker
docker info >/dev/null 2>&1 || die "Docker daemon is not reachable."
ensure_latency_helper

RUNTIME="${ATLASNET_LOCAL_LATENCY_RUNTIME:-$RUNTIME}"
STACK_NAME="${ATLASNET_SWARM_STACK_NAME:-atlasnet_dev}"
SHARD_SERVICE_NAME="${ATLASNET_SWARM_SHARD_SERVICE_NAME:-${STACK_NAME}_Shard}"
CLUSTER_NAME="${ATLASNET_K3D_CLUSTER_NAME:-atlasnet-dev}"
LATENCY_DELAY="${LATENCY_DELAY:-100ms}"
LATENCY_JITTER="${LATENCY_JITTER:-}"

if [[ "$ACTION" == "clear-all" && "$RUNTIME" == "auto" ]]; then
  resolve_swarm_shard_targets "$SHARD_SERVICE_NAME"
  resolve_k3d_worker_targets "$CLUSTER_NAME"
  helper_clear_latency "swarm" "${resolved_swarm_shards[@]}"
  helper_clear_latency "k3d" "${resolved_k3d_workers[@]}"
  exit 0
fi

if [[ "$RUNTIME" == "auto" ]]; then
  RUNTIME="$(detect_runtime "$SHARD_SERVICE_NAME" "$CLUSTER_NAME")"
fi

case "$RUNTIME" in
  swarm)
    if [[ "$ACTION" == "apply-auto" ]]; then
      resolve_swarm_shard_targets "$SHARD_SERVICE_NAME"
      ((${#resolved_swarm_shards[@]} > 0)) || die "No running shard containers found for service '${SHARD_SERVICE_NAME}'."
      helper_apply_latency "swarm" "$(pick_random_target resolved_swarm_shards)"
      exit 0
    fi

    resolve_swarm_shard_targets "$SHARD_SERVICE_NAME"
    helper_clear_latency "swarm" "${resolved_swarm_shards[@]}"
    exit 0
    ;;
  k3d)
    if [[ "$ACTION" == "apply-auto" ]]; then
      resolve_k3d_worker_targets "$CLUSTER_NAME"
      ((${#resolved_k3d_workers[@]} > 0)) || die "No running k3d worker containers found for cluster '${CLUSTER_NAME}'."
      helper_apply_latency "k3d" "$(pick_random_target resolved_k3d_workers)"
      exit 0
    fi

    resolve_k3d_worker_targets "$CLUSTER_NAME"
    helper_clear_latency "k3d" "${resolved_k3d_workers[@]}"
    exit 0
    ;;
  *)
    die "Invalid resolved runtime: $RUNTIME"
    ;;
esac
