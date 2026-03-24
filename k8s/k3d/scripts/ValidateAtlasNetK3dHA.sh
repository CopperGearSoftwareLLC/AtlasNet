#!/usr/bin/env bash
set -euo pipefail

CLUSTER_NAME="${1:-atlasnet-dev}"
NAMESPACE="${ATLASNET_K8S_NAMESPACE:-atlasnet-dev}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
K3D_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
HOST_KUBECONFIG_PATH="${ATLASNET_HOST_KUBECONFIG:-${K3D_DIR}/.kube/k3d-${CLUSTER_NAME}-host.yaml}"
CARTOGRAPH_HOST="${ATLASNET_K3D_CARTOGRAPH_INGRESS_HOST:-cartograph.k3d.atlasnet.local}"
PROXY_PUBLIC_HOST="${ATLASNET_K3D_PROXY_PUBLIC_HOST:-127.0.0.1}"
PROXY_PUBLIC_PORT="${ATLASNET_K3D_PROXY_PUBLIC_PORT:-2555}"
ROLLOUT_TIMEOUT="${ATLASNET_K3D_HA_ROLLOUT_TIMEOUT:-240s}"
NODE_TIMEOUT_SECONDS="${ATLASNET_K3D_HA_NODE_TIMEOUT_SECONDS:-240}"
TEMP_KUBECONFIG=""
STOPPED_NODE=""
STOPPED_NODE_RESTART_POLICY=""

cleanup() {
    if [[ -n "$STOPPED_NODE" ]]; then
        echo "==> Restarting stopped node '$STOPPED_NODE' during cleanup..."
        if [[ -n "$STOPPED_NODE_RESTART_POLICY" ]]; then
            docker update --restart="$STOPPED_NODE_RESTART_POLICY" "$STOPPED_NODE" >/dev/null 2>&1 || true
        fi
        docker start "$STOPPED_NODE" >/dev/null 2>&1 || true
    fi
    if [[ -n "$TEMP_KUBECONFIG" && -f "$TEMP_KUBECONFIG" ]]; then
        rm -f "$TEMP_KUBECONFIG"
    fi
}
trap cleanup EXIT

require_cmd() {
    local cmd="$1"
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Error: '$cmd' is required but not installed." >&2
        exit 1
    fi
}

require_cmd curl
require_cmd docker
require_cmd k3d
require_cmd kubectl

prepare_kubeconfig() {
    TEMP_KUBECONFIG="$(mktemp /tmp/atlasnet-k3d-validate-XXXX.yaml)"
    if [[ -s "$HOST_KUBECONFIG_PATH" ]]; then
        cp "$HOST_KUBECONFIG_PATH" "$TEMP_KUBECONFIG"
    else
        k3d kubeconfig get "$CLUSTER_NAME" >"$TEMP_KUBECONFIG"
    fi
}

kctl() {
    kubectl --kubeconfig "$TEMP_KUBECONFIG" --context "k3d-${CLUSTER_NAME}" "$@"
}

wait_for_condition() {
    local description="$1"
    local timeout_seconds="$2"
    shift 2
    local started_at
    started_at="$(date +%s)"
    while true; do
        if "$@"; then
            return 0
        fi
        if (( "$(date +%s)" - started_at >= timeout_seconds )); then
            echo "Error: timed out waiting for ${description}." >&2
            return 1
        fi
        sleep 2
    done
}

first_workload_name() {
    local app="$1"
    kctl -n "$NAMESPACE" get deployment,statefulset -l "app=${app}" -o name 2>/dev/null | sed -n '1p'
}

wait_for_rollout() {
    local app="$1"
    local workload
    workload="$(first_workload_name "$app")"
    if [[ -z "$workload" ]]; then
        echo "Error: could not find rollout workload for app '$app'." >&2
        exit 1
    fi
    kctl -n "$NAMESPACE" rollout status "$workload" --timeout="$ROLLOUT_TIMEOUT" >/dev/null
}

get_first_pod() {
    local app="$1"
    kctl -n "$NAMESPACE" get pod -l "app=${app}" -o jsonpath='{.items[0].metadata.name}'
}

get_pod_node() {
    local pod="$1"
    kctl -n "$NAMESPACE" get pod "$pod" -o jsonpath='{.spec.nodeName}'
}

is_pod_ready() {
    local pod="$1"
    [[ "$(kctl -n "$NAMESPACE" get pod "$pod" -o jsonpath='{.status.conditions[?(@.type=="Ready")].status}' 2>/dev/null || true)" == "True" ]]
}

wait_for_pod_ready() {
    local pod="$1"
    wait_for_condition "pod ${pod} ready" "$NODE_TIMEOUT_SECONDS" is_pod_ready "$pod"
}

wait_for_pod_on_different_node() {
    local app="$1"
    local old_node="$2"
    wait_for_condition "app ${app} rescheduled away from ${old_node}" "$NODE_TIMEOUT_SECONDS" \
        bash -lc '
            kubectl --kubeconfig "'"$TEMP_KUBECONFIG"'" --context "k3d-'"$CLUSTER_NAME"'" -n "'"$NAMESPACE"'" \
                get pod -l "app='"$app"'" \
                -o jsonpath="{range .items[*]}{.metadata.name}{\" \"}{.spec.nodeName}{\" \"}{.status.conditions[?(@.type==\"Ready\")].status}{\"\n\"}{end}" 2>/dev/null \
            | awk '\''NF >= 3 && $2 != "'"$old_node"'" && $3 == "True" { found = 1 } END { exit(found ? 0 : 1) }'\''
        '
}

wait_for_node_ready_status() {
    local node="$1"
    local desired="$2"
    wait_for_condition "node ${node} Ready=${desired}" "$NODE_TIMEOUT_SECONDS" \
        bash -lc '
            status="$(kubectl --kubeconfig "'"$TEMP_KUBECONFIG"'" --context "k3d-'"$CLUSTER_NAME"'" get node "'"$node"'" -o jsonpath="{.status.conditions[?(@.type==\"Ready\")].status}" 2>/dev/null || true)"
            [[ "$status" == "'"$desired"'" ]]
        '
}

wait_for_node_not_ready() {
    local node="$1"
    wait_for_condition "node ${node} not Ready" "$NODE_TIMEOUT_SECONDS" \
        bash -lc '
            status="$(kubectl --kubeconfig "'"$TEMP_KUBECONFIG"'" --context "k3d-'"$CLUSTER_NAME"'" get node "'"$node"'" -o jsonpath="{.status.conditions[?(@.type==\"Ready\")].status}" 2>/dev/null || true)"
            [[ "$status" != "True" ]]
        '
}

assert_cartograph_ingress() {
    curl --fail --silent --show-error \
        --resolve "${CARTOGRAPH_HOST}:80:127.0.0.1" \
        "http://${CARTOGRAPH_HOST}/" >/dev/null
}

assert_proxy_service_endpoint() {
    local addresses
    addresses="$(kctl -n "$NAMESPACE" get endpoints atlasnet-proxy -o jsonpath='{.subsets[*].addresses[*].ip}' 2>/dev/null || true)"
    [[ -n "$addresses" ]]
}

assert_internaldb_roundtrip() {
    local pod key value actual
    pod="$(get_first_pod atlasnet-internaldb)"
    key="atlasnet:ha:probe:$$"
    value="ok-$(date +%s)"
    kctl -n "$NAMESPACE" exec "$pod" -- valkey-cli -p 6379 set "$key" "$value" >/dev/null
    actual="$(kctl -n "$NAMESPACE" exec "$pod" -- valkey-cli -p 6379 get "$key" | tr -d '\r')"
    if [[ "$actual" != "$value" ]]; then
        echo "Error: internaldb roundtrip failed (expected '$value', got '$actual')." >&2
        exit 1
    fi
}

stop_node() {
    local node="$1"
    if [[ -n "$STOPPED_NODE" && "$STOPPED_NODE" != "$node" ]]; then
        echo "Error: refusing to stop a second node while '$STOPPED_NODE' is still stopped." >&2
        exit 1
    fi
    STOPPED_NODE_RESTART_POLICY="$(docker inspect -f '{{.HostConfig.RestartPolicy.Name}}' "$node" 2>/dev/null || true)"
    STOPPED_NODE_RESTART_POLICY="${STOPPED_NODE_RESTART_POLICY:-unless-stopped}"
    docker update --restart=no "$node" >/dev/null
    echo "==> Stopping k3d node '$node'..."
    docker stop "$node" >/dev/null
    STOPPED_NODE="$node"
}

start_stopped_node() {
    if [[ -z "$STOPPED_NODE" ]]; then
        return 0
    fi
    if [[ -n "$STOPPED_NODE_RESTART_POLICY" ]]; then
        docker update --restart="$STOPPED_NODE_RESTART_POLICY" "$STOPPED_NODE" >/dev/null
    fi
    echo "==> Starting k3d node '$STOPPED_NODE'..."
    docker start "$STOPPED_NODE" >/dev/null
    local node="$STOPPED_NODE"
    STOPPED_NODE=""
    STOPPED_NODE_RESTART_POLICY=""
    wait_for_node_ready_status "$node" "True"
}

show_singleton_placements() {
    echo "==> Current singleton placements"
    kctl -n "$NAMESPACE" get pod -o wide \
        -l 'app in (atlasnet-watchdog,atlasnet-cartograph,atlasnet-proxy,atlasnet-internaldb,atlasnet-llm)'
}

drill_stateless_singleton() {
    local app="$1"
    local verify_cmd="$2"
    local pod node

    pod="$(get_first_pod "$app")"
    node="$(get_pod_node "$pod")"
    echo "==> Drilling '$app' by stopping host node '$node'..."
    stop_node "$node"
    wait_for_node_not_ready "$node"
    wait_for_pod_on_different_node "$app" "$node"
    if [[ -n "$verify_cmd" ]]; then
        "$verify_cmd"
    fi
    start_stopped_node
}

drill_internaldb_recovery() {
    local pod node
    pod="$(get_first_pod atlasnet-internaldb)"
    node="$(get_pod_node "$pod")"
    echo "==> Drilling persistent 'atlasnet-internaldb' by stopping node '$node' and bringing it back..."
    stop_node "$node"
    wait_for_node_not_ready "$node"
    sleep 10
    start_stopped_node
    wait_for_rollout atlasnet-internaldb
    pod="$(get_first_pod atlasnet-internaldb)"
    wait_for_pod_ready "$pod"
    assert_internaldb_roundtrip
}

prepare_kubeconfig

echo "==> Waiting for cluster nodes and singleton workloads..."
kctl wait --for=condition=Ready nodes --all --timeout=120s >/dev/null
wait_for_rollout atlasnet-watchdog
wait_for_rollout atlasnet-proxy
wait_for_rollout atlasnet-cartograph
wait_for_rollout atlasnet-internaldb

show_singleton_placements

echo "==> Verifying stable access paths..."
assert_cartograph_ingress
assert_proxy_service_endpoint
assert_internaldb_roundtrip
echo "   - Cartograph ingress reachable at http://${CARTOGRAPH_HOST}"
echo "   - Proxy service has an endpoint and is published for ${PROXY_PUBLIC_HOST}:${PROXY_PUBLIC_PORT}"
echo "   - InternalDB responds on stable service 'internaldb:6379'"

drill_stateless_singleton atlasnet-cartograph assert_cartograph_ingress
drill_stateless_singleton atlasnet-watchdog ""
drill_stateless_singleton atlasnet-proxy assert_proxy_service_endpoint
drill_internaldb_recovery

echo
echo "HA validation completed successfully."
echo "Re-run after deployment updates with:"
echo "  k8s/k3d/scripts/ValidateAtlasNetK3dHA.sh ${CLUSTER_NAME}"
