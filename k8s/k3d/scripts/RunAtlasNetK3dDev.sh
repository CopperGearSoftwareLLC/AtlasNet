#!/usr/bin/env bash
set -euo pipefail

CLUSTER_NAME="${1:-atlasnet-dev}"
SHARD_IMAGE_NAME="${2:-sandbox-server:latest}"
NAMESPACE="${ATLASNET_K8S_NAMESPACE:-atlasnet-dev}"
SWARM_STACK_PREFIX="${ATLASNET_SWARM_STACK_PREFIX:-atlasnet_dev}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
K3D_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${K3D_DIR}/../.." && pwd)"
CHART_DIR="${REPO_ROOT}/k8s/charts/atlasnet"
K3D_SERVER_COUNT="${ATLASNET_K3D_SERVERS:-1}"
K3D_AGENT_COUNT="${ATLASNET_K3D_AGENTS:-2}"
PORT_WAIT_TIMEOUT="${ATLASNET_K3D_PORT_WAIT_TIMEOUT:-15}"
SERVER_NODE_NAME="${ATLASNET_SERVER_NODE_NAME:-k3d-${CLUSTER_NAME}-server-0}"
SKIP_RESTART_ON_FRESH_CLUSTER="${ATLASNET_K3D_SKIP_RESTART_ON_FRESH_CLUSTER:-1}"
CORE_ROLLOUT_MODE="${ATLASNET_K3D_CORE_ROLLOUT_MODE:-parallel}"
WAIT_FOR_SHARD_READY="${ATLASNET_K3D_WAIT_FOR_SHARD_READY:-1}"
IMAGE_PULL_POLICY="${ATLASNET_IMAGE_PULL_POLICY:-IfNotPresent}"
WATCHDOG_IMAGE_NAME="${ATLASNET_WATCHDOG_IMAGE:-watchdog:latest}"
PROXY_IMAGE_NAME="${ATLASNET_PROXY_IMAGE:-proxy:latest}"
CARTOGRAPH_IMAGE_NAME="${ATLASNET_CARTOGRAPH_IMAGE:-cartograph:latest}"
CARTOGRAPH_INGRESS_CLASS_NAME="${ATLASNET_K3D_CARTOGRAPH_INGRESS_CLASS_NAME:-traefik}"
CARTOGRAPH_INGRESS_HOST="${ATLASNET_K3D_CARTOGRAPH_INGRESS_HOST:-cartograph.k3d.atlasnet.local}"
CARTOGRAPH_LOOPBACK_PORT="${ATLASNET_K3D_CARTOGRAPH_LOOPBACK_PORT:-3000}"
CARTOGRAPH_LOOPBACK_PORT_FALLBACK="${ATLASNET_K3D_CARTOGRAPH_LOOPBACK_PORT_FALLBACK:-13000}"
CARTOGRAPH_INSPECT_PORT="${ATLASNET_K3D_CARTOGRAPH_INSPECT_PORT:-9229}"
CARTOGRAPH_INSPECT_PORT_FALLBACK="${ATLASNET_K3D_CARTOGRAPH_INSPECT_PORT_FALLBACK:-19229}"
HELM_RELEASE_NAME="${ATLASNET_HELM_RELEASE_NAME:-atlasnet}"
LLM_IN_CLUSTER_ENABLED="${ATLASNET_K3D_LLM_IN_CLUSTER_ENABLED:-1}"
LLM_IMAGE="${ATLASNET_K3D_LLM_IMAGE:-${ATLASNET_LLM_IMAGE:-ghcr.io/ggml-org/llama.cpp:server}}"
LLM_SERVICE_PORT="${ATLASNET_K3D_LLM_SERVICE_PORT:-8080}"
LLM_MODEL_URL="${ATLASNET_K3D_LLM_MODEL_URL:-${ATLASNET_LLM_MODEL_URL:-https://huggingface.co/Dannys0n/Qwen3-1.7B-seed_gen_voronoi/resolve/main/Qwen3-1.7B-seed_gen_voronoi-Q4_K_M.gguf}}"
LLM_MODEL_FILE_NAME="${ATLASNET_K3D_LLM_MODEL_FILE_NAME:-${ATLASNET_LLM_MODEL_FILE_NAME:-Qwen3-1.7B-seed_gen_voronoi-Q4_K_M.gguf}}"
LLM_ENDPOINT="${ATLASNET_K3D_LLM_ENDPOINT:-${ATLASNET_LLM_ENDPOINT:-}}"
LLM_API_FORMAT="${ATLASNET_K3D_LLM_API_FORMAT:-${ATLASNET_LLM_API_FORMAT:-openai}}"
LLM_MODEL_ID="${ATLASNET_K3D_LLM_MODEL_ID:-${ATLASNET_LLM_MODEL_ID:-huggingface.co/dannys0n/qwen3-1.7b-seed_gen_voronoi:Q4_K_M}}"
HOST_KUBECONFIG_PATH=""
ACTIVE_CARTOGRAPH_LOOPBACK_PORT=""
ACTIVE_CARTOGRAPH_INSPECT_PORT=""
PORT_FORWARD_PID_FILE=""
PORT_FORWARD_LOG_FILE=""

is_nonnegative_int() {
    [[ "$1" =~ ^[0-9]+$ ]]
}

if ! is_nonnegative_int "$K3D_SERVER_COUNT" || ((K3D_SERVER_COUNT < 1)); then
    echo "Error: ATLASNET_K3D_SERVERS must be an integer >= 1 (got '$K3D_SERVER_COUNT')." >&2
    exit 1
fi
if ! is_nonnegative_int "$K3D_AGENT_COUNT"; then
    echo "Error: ATLASNET_K3D_AGENTS must be an integer >= 0 (got '$K3D_AGENT_COUNT')." >&2
    exit 1
fi
if ! is_nonnegative_int "$PORT_WAIT_TIMEOUT" || ((PORT_WAIT_TIMEOUT < 1)); then
    echo "Error: ATLASNET_K3D_PORT_WAIT_TIMEOUT must be an integer >= 1 (got '$PORT_WAIT_TIMEOUT')." >&2
    exit 1
fi
if [[ -z "$SERVER_NODE_NAME" ]]; then
    echo "Error: ATLASNET_SERVER_NODE_NAME must not be empty." >&2
    exit 1
fi

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
require_cmd helm "Hint: install helm (devcontainer feature ghcr.io/devcontainers/features/kubectl-helm-minikube:1)."

if ! docker info >/dev/null 2>&1; then
    echo "Error: docker daemon is not reachable."
    exit 1
fi

if [[ ! -d "$CHART_DIR" ]]; then
    echo "Error: Helm chart not found at '$CHART_DIR'."
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
    local timeout="$PORT_WAIT_TIMEOUT"
    local elapsed=0
    local ports=(80 443 3000 9229 2555)
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
    ss -ltnu | grep -E ':80|:443|:3000|:9229|:2555' || true
    echo "Hint: free 80/443/3000/9229/2555 on the host, then retry." >&2
    echo "      In this repo, keep 3000/9229 out of static devcontainer forwardPorts to avoid k3d bind conflicts." >&2
    exit 1
}

CLUSTER_EXISTS=0
if k3d cluster get "$CLUSTER_NAME" >/dev/null 2>&1; then
    CLUSTER_EXISTS=1
fi

remove_swarm_leftovers

echo "==> Ensuring k3d cluster '$CLUSTER_NAME' exists (servers=${K3D_SERVER_COUNT}, agents=${K3D_AGENT_COUNT})..."
if ((CLUSTER_EXISTS == 0)); then
    wait_for_ports_free
    k3d cluster create "$CLUSTER_NAME" \
        --servers "$K3D_SERVER_COUNT" \
        --agents "$K3D_AGENT_COUNT" \
        --wait \
        -p "80:80@loadbalancer" \
        -p "443:443@loadbalancer" \
        -p "2555:2555/udp@loadbalancer" \
        --volume "/var/run/docker.sock:/var/run/docker.sock@all"
else
    echo "==> k3d cluster '$CLUSTER_NAME' already exists."
fi

write_host_kubeconfig() {
    local server_lb host_cfg api_port cluster server_url
    host_cfg="${ATLASNET_HOST_KUBECONFIG:-${K3D_DIR}/.kube/k3d-${CLUSTER_NAME}-host.yaml}"
    mkdir -p "$(dirname "$host_cfg")"

    if ! k3d kubeconfig get "$CLUSTER_NAME" >"$host_cfg"; then
        echo "Error: failed to write host kubeconfig for cluster '$CLUSTER_NAME' to '$host_cfg'." >&2
        exit 1
    fi
    if [[ ! -s "$host_cfg" ]]; then
        echo "Error: host kubeconfig file '$host_cfg' is empty after write." >&2
        exit 1
    fi
    chmod 600 "$host_cfg" 2>/dev/null || true

    server_lb="k3d-${CLUSTER_NAME}-serverlb"
    if docker ps --format '{{.Names}}' | grep -Fxq "$server_lb"; then
        api_port="$(docker inspect --format '{{with index .NetworkSettings.Ports "6443/tcp"}}{{(index . 0).HostPort}}{{end}}' "$server_lb" 2>/dev/null || true)"
    fi

    # Fallback: infer the API port from the kubeconfig endpoint when possible.
    if [[ -z "${api_port:-}" ]] && command -v kubectl >/dev/null 2>&1; then
        server_url="$(kubectl --kubeconfig "$host_cfg" config view --minify -o jsonpath='{.clusters[0].cluster.server}' 2>/dev/null || true)"
        if [[ "$server_url" =~ ^https://(0\.0\.0\.0|127\.0\.0\.1):([0-9]+)$ ]]; then
            api_port="${BASH_REMATCH[2]}"
        fi
    fi

    if command -v kubectl >/dev/null 2>&1; then
        cluster="$(kubectl --kubeconfig "$host_cfg" config view --minify -o jsonpath='{.contexts[0].context.cluster}' 2>/dev/null || true)"
        if [[ -n "$cluster" && -n "${api_port:-}" ]]; then
            kubectl --kubeconfig "$host_cfg" config set-cluster "$cluster" --server "https://127.0.0.1:${api_port}" >/dev/null 2>&1 || true
        fi
    fi

    if [[ -n "${api_port:-}" ]]; then
        # Fallback text rewrite in case kubectl config set-cluster is unavailable.
        sed -i -E "s#https://0\\.0\\.0\\.0:[0-9]+#https://127.0.0.1:${api_port}#g" "$host_cfg" || true
    fi
    HOST_KUBECONFIG_PATH="$host_cfg"

    echo "==> Host kubeconfig written: $host_cfg"
    if [[ -n "${api_port:-}" ]]; then
        echo "==> Host API endpoint: https://127.0.0.1:${api_port}"
    else
        echo "==> Host API endpoint: using k3d-emitted server address from kubeconfig."
    fi
    echo "==> Headlamp: import/use this kubeconfig file on your host."
}

write_host_kubeconfig

if [[ "$LLM_IN_CLUSTER_ENABLED" == "0" && -z "$LLM_ENDPOINT" ]]; then
    echo "Warning: k3d in-cluster LLM is disabled and no external LLM endpoint is set."
    echo "         Set ATLASNET_K3D_LLM_ENDPOINT or ATLASNET_LLM_ENDPOINT to use LlmVoronoi."
fi

is_pid_running() {
    local pid="${1:-}"
    [[ -n "$pid" ]] || return 1
    kill -0 "$pid" >/dev/null 2>&1
}

pick_loopback_port() {
    local preferred="$1"
    local fallback="$2"

    if ! port_in_use "$preferred"; then
        printf '%s\n' "$preferred"
        return 0
    fi
    if ! port_in_use "$fallback"; then
        printf '%s\n' "$fallback"
        return 0
    fi

    return 1
}

stop_existing_cartograph_port_forward() {
    local existing_pid=""

    if [[ -f "$PORT_FORWARD_PID_FILE" ]]; then
        existing_pid="$(<"$PORT_FORWARD_PID_FILE")"
        if is_pid_running "$existing_pid"; then
            kill "$existing_pid" >/dev/null 2>&1 || true
            for _attempt in {1..20}; do
                if ! is_pid_running "$existing_pid"; then
                    break
                fi
                sleep 0.1
            done
            if is_pid_running "$existing_pid"; then
                kill -9 "$existing_pid" >/dev/null 2>&1 || true
            fi
        fi
        rm -f "$PORT_FORWARD_PID_FILE"
    fi
}

ensure_cartograph_port_forward() {
    local pf_pid=""

    if ! command -v kubectl >/dev/null 2>&1; then
        echo "Warning: kubectl is not installed; skipping Cartograph loopback port-forward."
        return 0
    fi
    if ! command -v python3 >/dev/null 2>&1; then
        echo "Warning: python3 is not installed; skipping Cartograph loopback port-forward."
        return 0
    fi
    if [[ -z "${HOST_KUBECONFIG_PATH:-}" || ! -s "$HOST_KUBECONFIG_PATH" ]]; then
        echo "Warning: host kubeconfig is unavailable; skipping Cartograph loopback port-forward."
        return 0
    fi

    ACTIVE_CARTOGRAPH_LOOPBACK_PORT="$(pick_loopback_port "$CARTOGRAPH_LOOPBACK_PORT" "$CARTOGRAPH_LOOPBACK_PORT_FALLBACK")" \
        || { echo "Error: no free host port available for Cartograph HTTP loopback (${CARTOGRAPH_LOOPBACK_PORT}/${CARTOGRAPH_LOOPBACK_PORT_FALLBACK})." >&2; return 1; }
    ACTIVE_CARTOGRAPH_INSPECT_PORT="$(pick_loopback_port "$CARTOGRAPH_INSPECT_PORT" "$CARTOGRAPH_INSPECT_PORT_FALLBACK")" \
        || { echo "Error: no free host port available for Cartograph inspect loopback (${CARTOGRAPH_INSPECT_PORT}/${CARTOGRAPH_INSPECT_PORT_FALLBACK})." >&2; return 1; }

    if [[ "$ACTIVE_CARTOGRAPH_INSPECT_PORT" == "$ACTIVE_CARTOGRAPH_LOOPBACK_PORT" ]]; then
        echo "Error: selected Cartograph loopback ports collide on $ACTIVE_CARTOGRAPH_LOOPBACK_PORT." >&2
        return 1
    fi

    stop_existing_cartograph_port_forward

    : >"$PORT_FORWARD_LOG_FILE"
    rm -f "$PORT_FORWARD_PID_FILE"
    pf_pid="$(python3 - <<'PY' "$HOST_KUBECONFIG_PATH" "$NAMESPACE" "$ACTIVE_CARTOGRAPH_LOOPBACK_PORT" "$ACTIVE_CARTOGRAPH_INSPECT_PORT" "$PORT_FORWARD_LOG_FILE" "$PORT_FORWARD_PID_FILE"
import subprocess
import sys

kubeconfig, namespace, http_port, inspect_port, log_path, pid_path = sys.argv[1:]

with open(log_path, "ab", buffering=0) as log_fp, open("/dev/null", "rb", buffering=0) as devnull:
    proc = subprocess.Popen(
        [
            "kubectl",
            "--kubeconfig",
            kubeconfig,
            "-n",
            namespace,
            "port-forward",
            "service/atlasnet-cartograph",
            f"{http_port}:3000",
            f"{inspect_port}:9229",
        ],
        stdin=devnull,
        stdout=log_fp,
        stderr=subprocess.STDOUT,
        close_fds=True,
        start_new_session=True,
    )

with open(pid_path, "w", encoding="utf-8") as pid_fp:
    pid_fp.write(f"{proc.pid}\n")

print(proc.pid)
PY
)" || {
        echo "Error: failed to launch Cartograph loopback port-forward." >&2
        return 1
    }

    for _attempt in {1..50}; do
        if port_in_use "$ACTIVE_CARTOGRAPH_LOOPBACK_PORT" && port_in_use "$ACTIVE_CARTOGRAPH_INSPECT_PORT"; then
            return 0
        fi
        if ! is_pid_running "$pf_pid"; then
            break
        fi
        sleep 0.1
    done

    echo "Error: failed to establish Cartograph loopback port-forward." >&2
    if [[ -s "$PORT_FORWARD_LOG_FILE" ]]; then
        sed -n '1,80p' "$PORT_FORWARD_LOG_FILE" >&2
    fi
    return 1
}

sync_headlamp_default_kubeconfig() {
    if [[ "${ATLASNET_HEADLAMP_KUBECONFIG_SYNC:-1}" != "1" ]]; then
        return
    fi

    local source_cfg target_cfg source_ctx source_cluster source_user tmp_cfg default_kubectl_cfg
    local -a target_cfgs=()
    local -A seen_targets=()
    source_cfg="${HOST_KUBECONFIG_PATH:-}"
    if [[ -z "$source_cfg" || ! -f "$source_cfg" ]]; then
        return
    fi

    default_kubectl_cfg="$HOME/.kube/config"
    if [[ -n "${ATLASNET_HEADLAMP_KUBECONFIG_PATH:-}" ]]; then
        target_cfg="$ATLASNET_HEADLAMP_KUBECONFIG_PATH"
    else
        target_cfg="$default_kubectl_cfg"
    fi

    # Always keep both the preferred target and ~/.kube/config up to date.
    target_cfgs+=("$target_cfg" "$default_kubectl_cfg")

    source_ctx=""
    source_cluster=""
    source_user=""
    if command -v kubectl >/dev/null 2>&1; then
        source_ctx="$(kubectl --kubeconfig "$source_cfg" config current-context 2>/dev/null || true)"
        source_cluster="$(kubectl --kubeconfig "$source_cfg" config view --minify -o jsonpath='{.contexts[0].context.cluster}' 2>/dev/null || true)"
        source_user="$(kubectl --kubeconfig "$source_cfg" config view --minify -o jsonpath='{.contexts[0].context.user}' 2>/dev/null || true)"
    fi

    for target_cfg in "${target_cfgs[@]}"; do
        if [[ -n "${seen_targets[$target_cfg]:-}" ]]; then
            continue
        fi
        seen_targets["$target_cfg"]=1

        mkdir -p "$(dirname "$target_cfg")"
        touch "$target_cfg"

        if [[ "$source_cfg" == "$target_cfg" ]]; then
            echo "==> Headlamp/default kubeconfig already at $target_cfg"
            continue
        fi

        # Dedicated files get a full overwrite so host tools always see latest cluster config.
        if [[ "$target_cfg" != "$default_kubectl_cfg" ]]; then
            tmp_cfg="$(mktemp /tmp/atlasnet-headlamp-kubeconfig-XXXX.yaml)"
            cp "$source_cfg" "$tmp_cfg"
            chmod 600 "$tmp_cfg" 2>/dev/null || true
            mv "$tmp_cfg" "$target_cfg"
            echo "==> Headlamp kubeconfig file updated: $target_cfg"
            continue
        fi

        # Default kubectl config is merged to preserve other clusters.
        if ! command -v kubectl >/dev/null 2>&1; then
            cp "$source_cfg" "$target_cfg"
            chmod 600 "$target_cfg" 2>/dev/null || true
            echo "==> kubectl not found; copied kubeconfig to $target_cfg"
            continue
        fi

        KUBECONFIG="$target_cfg" kubectl config delete-context "$source_ctx" >/dev/null 2>&1 || true
        KUBECONFIG="$target_cfg" kubectl config delete-cluster "$source_cluster" >/dev/null 2>&1 || true
        if [[ -n "$source_user" ]]; then
            KUBECONFIG="$target_cfg" kubectl config unset "users.${source_user}" >/dev/null 2>&1 || true
        fi

        tmp_cfg="$(mktemp /tmp/atlasnet-headlamp-kubeconfig-XXXX.yaml)"
        KUBECONFIG="$target_cfg:$source_cfg" kubectl config view --flatten >"$tmp_cfg"
        chmod 600 "$tmp_cfg" 2>/dev/null || true
        mv "$tmp_cfg" "$target_cfg"

        if [[ -n "$source_ctx" ]]; then
            KUBECONFIG="$target_cfg" kubectl config use-context "$source_ctx" >/dev/null 2>&1 || true
        fi
        echo "==> Headlamp/default kubeconfig updated: $target_cfg"
    done
}

TEMP_KUBECONFIG="$(mktemp /tmp/k3d-kubeconfig-XXXX.yaml)"
TEMP_MANIFEST="$(mktemp /tmp/atlasnet-k3d-XXXX.yaml)"
trap 'rm -f "$TEMP_MANIFEST" "$TEMP_KUBECONFIG"' EXIT

KUBECTL_MODE=""
FORCE_INCLUSTER_KUBECTL="${ATLASNET_FORCE_INCLUSTER_KUBECTL:-0}"
KUBECTL=()

external_endpoint_tcp_reachable() {
    local server_url="$1"
    if ! [[ "$server_url" =~ ^https://([^/:]+):([0-9]+)$ ]]; then
        return 0
    fi

    # Skip this preflight if `timeout` is unavailable to avoid blocking sockets.
    if ! command -v timeout >/dev/null 2>&1; then
        return 0
    fi

    local host="${BASH_REMATCH[1]}"
    local port="${BASH_REMATCH[2]}"
    timeout 1 bash -c "cat </dev/null >/dev/tcp/${host}/${port}" >/dev/null 2>&1
}

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

    local endpoint probe_timeout_s
    endpoint="$(kubectl --kubeconfig "$TEMP_KUBECONFIG" config view --minify -o jsonpath='{.clusters[0].cluster.server}')"

    if ! external_endpoint_tcp_reachable "$endpoint"; then
        echo "==> External kube API endpoint not reachable via TCP from this environment."
        echo "==> Endpoint tried: $endpoint"
        return 1
    fi

    probe_timeout_s="${ATLASNET_EXTERNAL_KUBECTL_PROBE_TIMEOUT:-2s}"

    KUBECTL=(kubectl --kubeconfig "$TEMP_KUBECONFIG" --context "$context")
    echo "==> Using kubectl context '$context'..."
    if "${KUBECTL[@]}" --request-timeout="$probe_timeout_s" get --raw=/readyz >/dev/null 2>&1; then
        KUBECTL_MODE="external"
        return 0
    fi

    echo "==> External kube API endpoint not reachable from this environment."
    echo "==> Endpoint tried: $endpoint"
    return 1
}

setup_incluster_kubectl() {
    local server_node
    server_node="$SERVER_NODE_NAME"
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

ATLASNET_FORCE_IMAGE_IMPORT="${ATLASNET_FORCE_IMAGE_IMPORT:-0}"
IMAGE_CACHE_FILE="${ATLASNET_K3D_IMAGE_CACHE_FILE:-/tmp/atlasnet-k3d-${CLUSTER_NAME}-image-cache.txt}"
server_node="$SERVER_NODE_NAME"
current_cluster_id="$(docker inspect --format '{{.Id}}' "$server_node" 2>/dev/null || true)"
current_cluster_id="${current_cluster_id:-unknown}"

declare -A PREV_IMAGE_IDS=()
cached_cluster_id=""
if [[ -f "$IMAGE_CACHE_FILE" ]]; then
    while IFS='|' read -r image cached_id; do
        [[ -z "${image:-}" ]] && continue
        if [[ "$image" == "__cluster_id__" ]]; then
            cached_cluster_id="$cached_id"
            continue
        fi
        PREV_IMAGE_IDS["$image"]="$cached_id"
    done <"$IMAGE_CACHE_FILE"
fi
if [[ "$cached_cluster_id" != "$current_cluster_id" ]]; then
    PREV_IMAGE_IDS=()
fi

declare -a ROLLOUT_RESTART_APPS=()
declare -A RESTART_APP_SEEN=()
add_restart_app() {
    local app="$1"
    if [[ -n "${RESTART_APP_SEEN[$app]:-}" ]]; then
        return
    fi
    RESTART_APP_SEEN["$app"]=1
    ROLLOUT_RESTART_APPS+=("$app")
}

resolve_rollout_workloads() {
    local app="$1"
    kctl -n "$NAMESPACE" get deployment,statefulset,daemonset -l "app=${app}" -o name 2>/dev/null || true
}

restart_marked_apps() {
    local app workload found
    for app in "${ROLLOUT_RESTART_APPS[@]}"; do
        found=0
        while IFS= read -r workload; do
            [[ -z "$workload" ]] && continue
            found=1
            kctl -n "$NAMESPACE" rollout restart "$workload" >/dev/null || true
        done < <(resolve_rollout_workloads "$app")
        if ((found == 0)); then
            echo "   - warning: no rollout workload found for app '${app}'"
        fi
    done
}

wait_for_app_rollout() {
    local app="$1"
    local timeout="${2:-180s}"
    local workload found=0
    while IFS= read -r workload; do
        [[ -z "$workload" ]] && continue
        found=1
        kctl -n "$NAMESPACE" rollout status "$workload" --timeout="$timeout"
    done < <(resolve_rollout_workloads "$app")

    if ((found == 0)); then
        echo "Error: no rollout workload found for app '$app' in namespace '$NAMESPACE'." >&2
        exit 1
    fi
}

wait_for_apps_rollout_parallel() {
    local timeout="${1:-180s}"
    shift
    local -a apps=("$@")
    local -a pids=()
    local -a names=()
    local idx

    for app in "${apps[@]}"; do
        [[ -z "$app" ]] && continue
        (
            wait_for_app_rollout "$app" "$timeout"
        ) &
        pids+=("$!")
        names+=("$app")
    done

    for idx in "${!pids[@]}"; do
        if ! wait "${pids[$idx]}"; then
            echo "Error: rollout wait failed for app '${names[$idx]}'." >&2
            exit 1
        fi
    done
}

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
declare -A CURRENT_IMAGE_IDS=()
declare -a IMAGES_TO_IMPORT=()
for image in "${IMAGES[@]}"; do
    if [[ -n "${SEEN[$image]:-}" ]]; then
        continue
    fi
    SEEN["$image"]=1

    if docker image inspect "$image" >/dev/null 2>&1; then
        image_id="$(docker image inspect --format '{{.Id}}' "$image" 2>/dev/null || true)"
        if [[ -z "$image_id" ]]; then
            echo "   - skipping $image (unable to inspect image id)"
            continue
        fi

        CURRENT_IMAGE_IDS["$image"]="$image_id"
        needs_import=0
        if [[ "$ATLASNET_FORCE_IMAGE_IMPORT" == "1" ]]; then
            needs_import=1
        elif [[ "${PREV_IMAGE_IDS[$image]:-}" != "$image_id" ]]; then
            needs_import=1
        fi

        if ((needs_import == 1)); then
            echo "   - queued import $image"
            IMAGES_TO_IMPORT+=("$image")
            case "$image" in
                watchdog:*) add_restart_app "atlasnet-watchdog" ;;
                proxy:*) add_restart_app "atlasnet-proxy" ;;
                cartograph:*) add_restart_app "atlasnet-cartograph" ;;
            esac
            if [[ "$image" == "shard:latest" || "$image" == "$SHARD_IMAGE_NAME" ]]; then
                add_restart_app "atlasnet-shard"
            fi
        else
            echo "   - cached in cluster $image"
        fi
    else
        echo "   - skipping $image (not found locally)"
    fi
done

if ((${#IMAGES_TO_IMPORT[@]} > 0)); then
    IMPORT_MODE="${ATLASNET_K3D_IMAGE_IMPORT_MODE:-batch}"
    IMPORT_TIMEOUT="${ATLASNET_K3D_IMAGE_IMPORT_TIMEOUT:-0}"
    echo "   - importing ${#IMAGES_TO_IMPORT[@]} image(s) (mode: ${IMPORT_MODE})..."

    if [[ "$IMPORT_MODE" == "batch" ]]; then
        import_start="$(date +%s)"
        printf '   - batch image set: %s\n' "${IMAGES_TO_IMPORT[*]}"
        if [[ "$IMPORT_TIMEOUT" =~ ^[0-9]+$ ]] && ((IMPORT_TIMEOUT > 0)) && command -v timeout >/dev/null 2>&1; then
            timeout "${IMPORT_TIMEOUT}s" k3d image import -c "$CLUSTER_NAME" "${IMAGES_TO_IMPORT[@]}"
        else
            k3d image import -c "$CLUSTER_NAME" "${IMAGES_TO_IMPORT[@]}"
        fi
        import_elapsed="$(( $(date +%s) - import_start ))"
        echo "   - batch import complete (${import_elapsed}s)"
    else
        for image in "${IMAGES_TO_IMPORT[@]}"; do
            image_start="$(date +%s)"
            echo "   - importing $image ..."
            if [[ "$IMPORT_TIMEOUT" =~ ^[0-9]+$ ]] && ((IMPORT_TIMEOUT > 0)) && command -v timeout >/dev/null 2>&1; then
                timeout "${IMPORT_TIMEOUT}s" k3d image import -c "$CLUSTER_NAME" "$image"
            else
                k3d image import -c "$CLUSTER_NAME" "$image"
            fi
            image_elapsed="$(( $(date +%s) - image_start ))"
            echo "   - imported $image (${image_elapsed}s)"
        done
    fi
else
    echo "   - no image imports required"
fi

mkdir -p "$(dirname "$IMAGE_CACHE_FILE")"
{
    printf '__cluster_id__|%s\n' "$current_cluster_id"
    for image in "${!CURRENT_IMAGE_IDS[@]}"; do
        printf '%s|%s\n' "$image" "${CURRENT_IMAGE_IDS[$image]}"
    done | sort
} >"$IMAGE_CACHE_FILE"

helm template "$HELM_RELEASE_NAME" "$CHART_DIR" \
    --namespace "$NAMESPACE" \
    --set-string serverNodeName="$SERVER_NODE_NAME" \
    --set-string imagePullPolicy="$IMAGE_PULL_POLICY" \
    --set-string images.watchdog="$WATCHDOG_IMAGE_NAME" \
    --set-string images.proxy="$PROXY_IMAGE_NAME" \
    --set-string images.shard="$SHARD_IMAGE_NAME" \
    --set-string images.cartograph="$CARTOGRAPH_IMAGE_NAME" \
    --set-string cartograph.ingress.className="$CARTOGRAPH_INGRESS_CLASS_NAME" \
    --set-string cartograph.ingress.host="$CARTOGRAPH_INGRESS_HOST" \
    --set llm.enabled="$LLM_IN_CLUSTER_ENABLED" \
    --set-string llm.image="$LLM_IMAGE" \
    --set-string llm.servicePort="$LLM_SERVICE_PORT" \
    --set-string llm.endpoint="$LLM_ENDPOINT" \
    --set-string llm.apiFormat="$LLM_API_FORMAT" \
    --set-string llm.modelId="$LLM_MODEL_ID" \
    --set-string llm.modelUrl="$LLM_MODEL_URL" \
    --set-string llm.modelFileName="$LLM_MODEL_FILE_NAME" \
    >"$TEMP_MANIFEST"

if [[ "$KUBECTL_MODE" == "incluster" ]]; then
    printf 'apiVersion: v1\nkind: Namespace\nmetadata:\n  name: %s\n' "$NAMESPACE" | "${KUBECTL[@]}" apply -f - >/dev/null
else
    kctl create namespace "$NAMESPACE" --dry-run=client -o yaml | kctl apply -f - >/dev/null
fi

echo "==> Applying Kubernetes manifest..."
if [[ "$KUBECTL_MODE" == "incluster" ]]; then
    cat "$TEMP_MANIFEST" | "${KUBECTL[@]}" apply -f - >/dev/null
else
    kctl apply -f "$TEMP_MANIFEST" >/dev/null
fi
# Migration cleanup: workloads moved kinds across revisions.
kctl -n "$NAMESPACE" delete daemonset atlasnet-watchdog --ignore-not-found >/dev/null || true
kctl -n "$NAMESPACE" delete deployment atlasnet-cartograph --ignore-not-found >/dev/null || true
kctl -n "$NAMESPACE" delete deployment atlasnet-proxy --ignore-not-found >/dev/null || true
# Shard deployment is declared with replicas=0 and is expected to be scaled by Watchdog.
# On a fresh cluster, initial apply already starts the latest pods; skip extra rollout restarts by default.
if ((CLUSTER_EXISTS == 0)) && [[ "$SKIP_RESTART_ON_FRESH_CLUSTER" == "1" ]]; then
    echo "==> Fresh cluster detected; skipping post-apply rollout restarts."
else
    # Always restart watchdog on non-fresh runs so it recomputes shard target each run.
    add_restart_app "atlasnet-watchdog"
    echo "==> Restarting updated workloads..."
    restart_marked_apps
fi
echo "==> Waiting for core workloads..."
if [[ "$CORE_ROLLOUT_MODE" == "parallel" ]]; then
    wait_for_apps_rollout_parallel "180s" \
        "atlasnet-internaldb" \
        "atlasnet-watchdog" \
        "atlasnet-proxy" \
        "atlasnet-cartograph"
else
    wait_for_app_rollout "atlasnet-internaldb" "180s"
    wait_for_app_rollout "atlasnet-watchdog" "180s"
    wait_for_app_rollout "atlasnet-proxy" "180s"
    wait_for_app_rollout "atlasnet-cartograph" "180s"
fi

if [[ "$WAIT_FOR_SHARD_READY" == "1" ]]; then
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
else
    echo "==> Skipping watchdog-driven shard scale-up wait (ATLASNET_K3D_WAIT_FOR_SHARD_READY=$WAIT_FOR_SHARD_READY)."
fi

sync_headlamp_default_kubeconfig
PORT_FORWARD_PID_FILE="${ATLASNET_K3D_CARTOGRAPH_PORT_FORWARD_PID_FILE:-/tmp/atlasnet-k3d-${CLUSTER_NAME}-cartograph-port-forward.pid}"
PORT_FORWARD_LOG_FILE="${ATLASNET_K3D_CARTOGRAPH_PORT_FORWARD_LOG_FILE:-/tmp/atlasnet-k3d-${CLUSTER_NAME}-cartograph-port-forward.log}"
echo "==> Ensuring Cartograph loopback port-forward..."
ensure_cartograph_port_forward

echo "==> Ensuring latency helper is running..."
bash "${REPO_ROOT}/Dev/EnsureLatencyHelper.sh"

echo
echo "Kubernetes services in namespace '$NAMESPACE':"
kctl get svc -n "$NAMESPACE"
echo
echo "Cluster '$CLUSTER_NAME' is ready."
echo "Cartograph: http://${CARTOGRAPH_INGRESS_HOST}"
echo "Cartograph loopback: http://127.0.0.1:${ACTIVE_CARTOGRAPH_LOOPBACK_PORT}"
echo "Cartograph inspect loopback: 127.0.0.1:${ACTIVE_CARTOGRAPH_INSPECT_PORT}"
echo "Proxy UDP: 127.0.0.1:2555"
echo "InternalDB: Cluster-internal service (internaldb:6379)"
