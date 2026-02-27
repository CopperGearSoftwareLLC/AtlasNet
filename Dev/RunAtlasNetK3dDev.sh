#!/usr/bin/env bash
set -euo pipefail

CLUSTER_NAME="${1:-atlasnet-dev}"
SHARD_IMAGE_NAME="${2:-sandbox-server:latest}"
NAMESPACE="${ATLASNET_K8S_NAMESPACE:-atlasnet-dev}"
SWARM_STACK_PREFIX="${ATLASNET_SWARM_STACK_PREFIX:-atlasnet_dev}"
MANIFEST_TEMPLATE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/deploy/k8s/overlays/k3d/atlasnet-dev.yaml"

require_cmd() {
    local cmd="$1"
    local hint="${2:-}"
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Error: '$cmd' is required but not installed."
        if [[ -n "$hint" ]]; then
            echo "$hint"
        fi
        exit 1
    fi
}

require_cmd docker "Hint: ensure Docker CLI is installed and /var/run/docker.sock is mounted."
require_cmd k3d "Hint: rebuild devcontainer with feature ghcr.io/rio/features/k3d:1 enabled."

if ! docker info >/dev/null 2>&1; then
    echo "Error: docker daemon is not reachable."
    exit 1
fi

if [[ ! -f "$MANIFEST_TEMPLATE" ]]; then
    echo "Error: manifest template not found at '$MANIFEST_TEMPLATE'."
    exit 1
fi

remove_swarm_leftovers() {
    local swarm_state
    swarm_state="$(docker info --format '{{.Swarm.LocalNodeState}}' 2>/dev/null || echo inactive)"
    if [[ "$swarm_state" != "active" ]]; then
        return
    fi

    mapfile -t services < <(
        docker service ls --format '{{.Name}}' 2>/dev/null | grep "^${SWARM_STACK_PREFIX}_" || true
    )
    if ((${#services[@]} == 0)); then
        return
    fi

    echo "==> Removing leftover Swarm services (${SWARM_STACK_PREFIX}_*)..."
    for svc in "${services[@]}"; do
        docker service rm "$svc" >/dev/null || true
    done

    local timeout=60
    local elapsed=0
    while ((elapsed < timeout)); do
        if ! docker service ls --format '{{.Name}}' 2>/dev/null | grep -q "^${SWARM_STACK_PREFIX}_"; then
            break
        fi
        sleep 1
        elapsed=$((elapsed + 1))
    done
}

port_in_use() {
    local port="$1"
    if ss -ltnu "( sport = :${port} )" 2>/dev/null | awk 'NR>1 {found=1} END {exit !found}'; then
        return 0
    fi
    return 1
}

wait_for_ports_free() {
    local timeout=60
    local elapsed=0
    local ports=(3000 9229 2555)
    while ((elapsed < timeout)); do
        local busy=0
        for p in "${ports[@]}"; do
            if port_in_use "$p"; then
                busy=1
                break
            fi
        done

        if ((busy == 0)); then
            return
        fi
        sleep 1
        elapsed=$((elapsed + 1))
    done

    echo "Error: ports are still busy after ${timeout}s; aborting k3d cluster startup." >&2
    ss -ltnu | grep -E ':3000|:9229|:2555' || true
    exit 1
}

CLUSTER_EXISTS=0
if k3d cluster get "$CLUSTER_NAME" >/dev/null 2>&1; then
    CLUSTER_EXISTS=1
fi

remove_swarm_leftovers

echo "==> Ensuring k3d cluster '$CLUSTER_NAME' exists..."
if ((CLUSTER_EXISTS == 0)); then
    wait_for_ports_free
    k3d cluster create "$CLUSTER_NAME" \
        --servers 1 \
        --agents 0 \
        --wait \
        -p "3000:3000@loadbalancer" \
        -p "9229:9229@loadbalancer" \
        -p "2555:2555@loadbalancer" \
        -p "2555:2555/udp@loadbalancer" \
        --volume "/var/run/docker.sock:/var/run/docker.sock@all"
else
    echo "==> k3d cluster '$CLUSTER_NAME' already exists."
fi

write_host_kubeconfig() {
    local server_lb host_cfg api_port
    server_lb="k3d-${CLUSTER_NAME}-serverlb"
    if ! docker ps --format '{{.Names}}' | grep -Fxq "$server_lb"; then
        return
    fi

    api_port="$(docker inspect --format '{{with index .NetworkSettings.Ports "6443/tcp"}}{{(index . 0).HostPort}}{{end}}' "$server_lb" 2>/dev/null || true)"
    if [[ -z "$api_port" ]]; then
        return
    fi

    host_cfg="${ATLASNET_HOST_KUBECONFIG:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/Dev/.kube/k3d-${CLUSTER_NAME}-host.yaml}"
    mkdir -p "$(dirname "$host_cfg")"

    k3d kubeconfig get "$CLUSTER_NAME" >"$host_cfg"
    if command -v kubectl >/dev/null 2>&1; then
        local cluster
        cluster="$(kubectl --kubeconfig "$host_cfg" config view --minify -o jsonpath='{.contexts[0].context.cluster}' 2>/dev/null || true)"
        if [[ -n "$cluster" ]]; then
            kubectl --kubeconfig "$host_cfg" config set-cluster "$cluster" --server "https://127.0.0.1:${api_port}" >/dev/null 2>&1 || true
        fi
    fi
    # Fallback text rewrite in case kubectl config set-cluster is unavailable.
    sed -i -E "s#https://0\\.0\\.0\\.0:[0-9]+#https://127.0.0.1:${api_port}#g" "$host_cfg" || true

    echo "==> Host kubeconfig written: $host_cfg"
    echo "==> Host API endpoint: https://127.0.0.1:${api_port}"
    echo "==> Headlamp: import/use this kubeconfig file on your host."
}

write_host_kubeconfig

TEMP_KUBECONFIG="$(mktemp /tmp/k3d-kubeconfig-XXXX.yaml)"
TEMP_MANIFEST="$(mktemp /tmp/atlasnet-k3d-XXXX.yaml)"
trap 'rm -f "$TEMP_MANIFEST" "$TEMP_KUBECONFIG"' EXIT

KUBECTL_MODE=""
FORCE_INCLUSTER_KUBECTL="${ATLASNET_FORCE_INCLUSTER_KUBECTL:-0}"
KUBECTL=()

setup_external_kubectl() {
    if [[ "$FORCE_INCLUSTER_KUBECTL" == "1" ]]; then
        return 1
    fi
    if ! command -v kubectl >/dev/null 2>&1; then
        return 1
    fi

    k3d kubeconfig get "$CLUSTER_NAME" >"$TEMP_KUBECONFIG"
    local context cluster server_url
    context="$(kubectl --kubeconfig "$TEMP_KUBECONFIG" config current-context)"
    cluster="$(kubectl --kubeconfig "$TEMP_KUBECONFIG" config view --minify -o jsonpath='{.contexts[0].context.cluster}')"
    server_url="$(kubectl --kubeconfig "$TEMP_KUBECONFIG" config view --minify -o jsonpath='{.clusters[0].cluster.server}')"

    # In dev containers k3d can emit 0.0.0.0:<api-port>, which is not a reachable destination.
    if [[ "$server_url" =~ ^https://0\.0\.0\.0:([0-9]+)$ ]]; then
        local api_port api_host fixed_server
        api_port="${BASH_REMATCH[1]}"
        api_host="127.0.0.1"
        if getent hosts host.docker.internal >/dev/null 2>&1; then
            api_host="host.docker.internal"
        fi
        fixed_server="https://${api_host}:${api_port}"
        echo "==> Rewriting kube API endpoint '$server_url' -> '$fixed_server'"
        kubectl --kubeconfig "$TEMP_KUBECONFIG" config set-cluster "$cluster" --server "$fixed_server" >/dev/null
    fi

    KUBECTL=(kubectl --kubeconfig "$TEMP_KUBECONFIG" --context "$context")
    echo "==> Using kubectl context '$context'..."
    for i in $(seq 1 60); do
        if "${KUBECTL[@]}" --request-timeout=3s get --raw=/readyz >/dev/null 2>&1; then
            KUBECTL_MODE="external"
            return 0
        fi
        sleep 1
    done

    echo "==> External kube API endpoint not reachable from this environment."
    echo "==> Endpoint tried: $(kubectl --kubeconfig "$TEMP_KUBECONFIG" config view --minify -o jsonpath='{.clusters[0].cluster.server}')"
    return 1
}

setup_incluster_kubectl() {
    local server_node
    server_node="k3d-${CLUSTER_NAME}-server-0"
    if ! docker ps --format '{{.Names}}' | grep -Fxq "$server_node"; then
        return 1
    fi

    KUBECTL=(docker exec -i "$server_node" kubectl --kubeconfig /etc/rancher/k3s/k3s.yaml)
    if ! "${KUBECTL[@]}" --request-timeout=3s get --raw=/readyz >/dev/null 2>&1; then
        return 1
    fi

    KUBECTL_MODE="incluster"
    echo "==> Using in-cluster kubectl via node '$server_node'."
    return 0
}

if ! setup_external_kubectl; then
    if ! setup_incluster_kubectl; then
        echo "Error: Kubernetes API for cluster '$CLUSTER_NAME' is not reachable via external or in-cluster kubectl." >&2
        if ! command -v kubectl >/dev/null 2>&1; then
            echo "Hint: install kubectl (devcontainer feature ghcr.io/devcontainers/features/kubectl-helm-minikube:1)." >&2
        fi
        exit 1
    fi
fi

kctl() {
    "${KUBECTL[@]}" "$@"
}

kctl wait --for=condition=Ready nodes --all --timeout=120s >/dev/null

echo "==> Importing local Docker images into k3d..."
declare -a IMAGES=(
    "watchdog:latest"
    "proxy:latest"
    "shard:latest"
    "cartograph:latest"
    "valkey/valkey-bundle:latest"
    "$SHARD_IMAGE_NAME"
)
declare -A SEEN=()
for image in "${IMAGES[@]}"; do
    if [[ -n "${SEEN[$image]:-}" ]]; then
        continue
    fi
    SEEN["$image"]=1

    if docker image inspect "$image" >/dev/null 2>&1; then
        echo "   - importing $image"
        k3d image import -c "$CLUSTER_NAME" "$image" >/dev/null
    else
        echo "   - skipping $image (not found locally)"
    fi
done

sed \
    -e "s|__NAMESPACE__|$NAMESPACE|g" \
    -e "s|__SHARD_IMAGE__|$SHARD_IMAGE_NAME|g" \
    "$MANIFEST_TEMPLATE" >"$TEMP_MANIFEST"

echo "==> Applying Kubernetes manifest..."
if [[ "$KUBECTL_MODE" == "incluster" ]]; then
    cat "$TEMP_MANIFEST" | "${KUBECTL[@]}" apply -f - >/dev/null
else
    kctl apply -f "$TEMP_MANIFEST" >/dev/null
fi
# Shard deployment is declared with replicas=0 and is expected to be scaled by Watchdog.
# Restart Watchdog after apply so it recomputes heuristic + shard target for each run.
kctl -n "$NAMESPACE" rollout restart deployment/atlasnet-watchdog >/dev/null
# Remove a legacy service name from earlier k3d migration iterations.
kctl -n "$NAMESPACE" delete svc atlasnet-internaldb --ignore-not-found >/dev/null || true

echo "==> Waiting for core deployments..."
kctl -n "$NAMESPACE" rollout status deployment/atlasnet-internaldb --timeout=180s
kctl -n "$NAMESPACE" rollout status deployment/atlasnet-watchdog --timeout=180s
kctl -n "$NAMESPACE" rollout status deployment/atlasnet-proxy --timeout=180s
kctl -n "$NAMESPACE" rollout status deployment/atlasnet-cartograph --timeout=180s

echo "==> Waiting for watchdog-driven shard scale-up..."
shard_ready=false
for _attempt in {1..90}; do
    desired="$(kctl -n "$NAMESPACE" get deployment atlasnet-shard -o jsonpath='{.spec.replicas}' 2>/dev/null || echo 0)"
    ready="$(kctl -n "$NAMESPACE" get deployment atlasnet-shard -o jsonpath='{.status.readyReplicas}' 2>/dev/null || echo 0)"
    desired="${desired:-0}"
    ready="${ready:-0}"

    if [[ "$desired" =~ ^[0-9]+$ && "$ready" =~ ^[0-9]+$ && "$desired" -gt 0 && "$ready" -ge 1 ]]; then
        echo "   - shard deployment desired=${desired}, ready=${ready}"
        shard_ready=true
        break
    fi
    sleep 2
done

if [[ "$shard_ready" != true ]]; then
    echo "Warning: shard deployment did not report a ready replica within timeout."
fi

echo
echo "Kubernetes services in namespace '$NAMESPACE':"
kctl get svc -n "$NAMESPACE"
echo
echo "Cluster '$CLUSTER_NAME' is ready."
echo "Cartograph: http://127.0.0.1:3000"
echo "Proxy TCP/UDP: 127.0.0.1:2555"
echo "InternalDB: Cluster-internal service (internaldb:6379)"
