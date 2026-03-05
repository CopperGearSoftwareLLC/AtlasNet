#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  Dev/FreePortsAndServices.sh [--dry-run] [--keep-k3d-clusters] [--keep-k8s-services] [--keep-local-listeners] [port ...]

Frees host ports by:
  1) Deleting k3d clusters whose load balancer publishes those ports
  2) Removing Docker Swarm services publishing those ports
  3) Removing Docker containers publishing those ports
  4) Deleting Kubernetes Services exposing those ports (service port or nodePort)
  5) Killing remaining local processes listening on those ports

Default ports (if none provided):
  2555 3000 9229

Examples:
  Dev/FreePortsAndServices.sh
  Dev/FreePortsAndServices.sh --dry-run
  Dev/FreePortsAndServices.sh --keep-k3d-clusters --keep-k8s-services
  Dev/FreePortsAndServices.sh --keep-local-listeners
  Dev/FreePortsAndServices.sh 3000 2555
EOF
}

DRY_RUN=0
KEEP_K3D_CLUSTERS=0
KEEP_K8S_SERVICES=0
KEEP_LOCAL_LISTENERS=0
declare -a INPUT_PORTS=()
declare -a DEFAULT_PORTS=(2555 3000 9229)

while (($# > 0)); do
    case "$1" in
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        --keep-k3d-clusters)
            KEEP_K3D_CLUSTERS=1
            shift
            ;;
        --keep-k8s-services)
            KEEP_K8S_SERVICES=1
            shift
            ;;
        --keep-local-listeners)
            KEEP_LOCAL_LISTENERS=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            INPUT_PORTS+=("$1")
            shift
            ;;
    esac
done

if ((${#INPUT_PORTS[@]} == 0)); then
    INPUT_PORTS=("${DEFAULT_PORTS[@]}")
fi

declare -A PORT_SET=()
for raw in "${INPUT_PORTS[@]}"; do
    if ! [[ "$raw" =~ ^[0-9]+$ ]]; then
        echo "Invalid port: '$raw' (must be an integer)." >&2
        exit 1
    fi
    if ((raw < 1 || raw > 65535)); then
        echo "Invalid port: '$raw' (must be 1-65535)." >&2
        exit 1
    fi
    PORT_SET["$raw"]=1
done

mapfile -t PORTS < <(printf '%s\n' "${!PORT_SET[@]}" | sort -n)

log() {
    echo "[free-ports] $*"
}

run_cmd() {
    if ((DRY_RUN)); then
        printf '[dry-run] '
        printf '%q ' "$@"
        printf '\n'
    else
        "$@"
    fi
}

docker_available() {
    command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1
}

delete_k3d_clusters_for_ports() {
    if ! docker_available; then
        log "Docker is not available/reachable; skipping k3d cluster checks."
        return
    fi
    if ! command -v k3d >/dev/null 2>&1; then
        log "k3d CLI not found; skipping k3d cluster checks."
        return
    fi

    mapfile -t lb_containers < <(docker ps -q \
        --filter "label=app=k3d" \
        --filter "label=k3d.role=loadbalancer")
    if ((${#lb_containers[@]} == 0)); then
        return
    fi

    declare -A clusters_to_delete=()
    for cid in "${lb_containers[@]}"; do
        local cluster
        cluster="$(docker inspect --format '{{ index .Config.Labels "k3d.cluster" }}' "$cid" 2>/dev/null || true)"
        [[ -z "$cluster" ]] && continue

        mapfile -t host_ports < <(
            docker inspect --format '{{range $port, $arr := .HostConfig.PortBindings}}{{if $arr}}{{(index $arr 0).HostPort}}{{"\n"}}{{end}}{{end}}' "$cid" 2>/dev/null || true
        )

        for hp in "${host_ports[@]}"; do
            [[ -z "$hp" ]] && continue
            if [[ -n "${PORT_SET[$hp]:-}" ]]; then
                clusters_to_delete["$cluster"]=1
                break
            fi
        done
    done

    for cluster in "${!clusters_to_delete[@]}"; do
        log "Deleting k3d cluster '$cluster' (publishes one of: ${PORTS[*]})."
        run_cmd k3d cluster delete "$cluster"
    done
}

delete_swarm_services_for_ports() {
    if ! docker_available; then
        log "Docker is not available/reachable; skipping Swarm service checks."
        return
    fi

    local swarm_state
    swarm_state="$(docker info --format '{{.Swarm.LocalNodeState}}' 2>/dev/null || echo inactive)"
    if [[ "$swarm_state" != "active" ]]; then
        return
    fi

    mapfile -t service_ids < <(docker service ls -q 2>/dev/null || true)
    if ((${#service_ids[@]} == 0)); then
        return
    fi

    declare -A services_to_delete=()
    for sid in "${service_ids[@]}"; do
        local svc_name
        svc_name="$(docker service inspect --format '{{.Spec.Name}}' "$sid" 2>/dev/null || true)"
        [[ -z "$svc_name" ]] && continue

        mapfile -t published_ports < <(
            docker service inspect --format '{{range .Endpoint.Spec.Ports}}{{.PublishedPort}}{{"\n"}}{{end}}' "$sid" 2>/dev/null || true
        )
        for p in "${published_ports[@]}"; do
            [[ -z "$p" ]] && continue
            if [[ -n "${PORT_SET[$p]:-}" ]]; then
                services_to_delete["$svc_name"]=1
                break
            fi
        done
    done

    for svc in "${!services_to_delete[@]}"; do
        log "Removing Docker Swarm service '$svc'."
        run_cmd docker service rm "$svc"
    done
}

delete_docker_containers_for_ports() {
    if ! docker_available; then
        log "Docker is not available/reachable; skipping container checks."
        return
    fi

    declare -A containers_to_delete=()
    for p in "${PORTS[@]}"; do
        while IFS= read -r cid; do
            [[ -z "$cid" ]] && continue
            containers_to_delete["$cid"]=1
        done < <(docker ps -q --filter "publish=$p")
    done

    for cid in "${!containers_to_delete[@]}"; do
        local name
        local app_label
        name="$(docker inspect --format '{{.Name}}' "$cid" 2>/dev/null | sed 's#^/##' || true)"
        app_label="$(docker inspect --format '{{ index .Config.Labels "app" }}' "$cid" 2>/dev/null || true)"

        # k3d infra should be deleted at cluster level, not per-container.
        if [[ "$app_label" == "k3d" ]]; then
            continue
        fi

        log "Removing Docker container '${name:-$cid}' ($cid)."
        run_cmd docker rm -f "$cid"
    done
}

delete_k8s_services_for_ports() {
    if ! command -v kubectl >/dev/null 2>&1; then
        log "kubectl not found; skipping Kubernetes service checks."
        return
    fi

    mapfile -t contexts < <(kubectl config get-contexts -o name 2>/dev/null || true)
    if ((${#contexts[@]} == 0)); then
        log "No kubectl contexts found; skipping Kubernetes service checks."
        return
    fi

    declare -A services_to_delete=()
    for ctx in "${contexts[@]}"; do
        local lines
        lines="$(
            kubectl --context "$ctx" --request-timeout=6s get svc -A \
                -o go-template='{{range .items}}{{ $ns := .metadata.namespace }}{{ $name := .metadata.name }}{{range .spec.ports}}{{printf "%s|%s|%v|%v\n" $ns $name .port .nodePort}}{{end}}{{end}}' \
                2>/dev/null || true
        )"

        while IFS='|' read -r ns name port nodeport; do
            [[ -z "${ns:-}" || -z "${name:-}" ]] && continue
            # Skip the core apiserver service object.
            if [[ "$ns/$name" == "default/kubernetes" ]]; then
                continue
            fi
            if [[ -n "${PORT_SET[$port]:-}" || -n "${PORT_SET[$nodeport]:-}" ]]; then
                services_to_delete["$ctx|$ns|$name"]=1
            fi
        done <<< "$lines"
    done

    for key in "${!services_to_delete[@]}"; do
        IFS='|' read -r ctx ns name <<< "$key"
        log "Deleting Kubernetes service '$ns/$name' in context '$ctx'."
        run_cmd kubectl --context "$ctx" delete svc -n "$ns" "$name"
    done
}

kill_remaining_local_listeners() {
    if ! command -v lsof >/dev/null 2>&1; then
        log "lsof not found; cannot kill local listener PIDs."
        return
    fi

    declare -A pids_to_kill=()
    for p in "${PORTS[@]}"; do
        while IFS= read -r pid; do
            [[ -z "$pid" ]] && continue
            pids_to_kill["$pid"]=1
        done < <(lsof -nP -t -iTCP:"$p" -sTCP:LISTEN 2>/dev/null || true)

        while IFS= read -r pid; do
            [[ -z "$pid" ]] && continue
            pids_to_kill["$pid"]=1
        done < <(lsof -nP -t -iUDP:"$p" 2>/dev/null || true)
    done

    for pid in "${!pids_to_kill[@]}"; do
        [[ "$pid" == "$$" ]] && continue
        local cmd
        cmd="$(ps -p "$pid" -o comm= 2>/dev/null || echo unknown)"

        if ((DRY_RUN)); then
            log "Would kill PID $pid ($cmd)."
            continue
        fi

        log "Killing PID $pid ($cmd)."
        kill -TERM "$pid" 2>/dev/null || true
        sleep 0.2
        if kill -0 "$pid" 2>/dev/null; then
            kill -KILL "$pid" 2>/dev/null || true
        fi
    done
}

log "Requested ports: ${PORTS[*]}"
((DRY_RUN)) && log "Dry-run mode enabled; no changes will be made."
((KEEP_K3D_CLUSTERS)) && log "Keeping existing k3d clusters."
((KEEP_K8S_SERVICES)) && log "Keeping existing Kubernetes Services."
((KEEP_LOCAL_LISTENERS)) && log "Keeping local listener processes."

if ((KEEP_K3D_CLUSTERS == 0)); then
    delete_k3d_clusters_for_ports
fi
delete_swarm_services_for_ports
delete_docker_containers_for_ports
if ((KEEP_K8S_SERVICES == 0)); then
    delete_k8s_services_for_ports
fi
if ((KEEP_LOCAL_LISTENERS == 0)); then
    kill_remaining_local_listeners
fi

log "Done."
