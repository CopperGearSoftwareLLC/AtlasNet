#!/usr/bin/env bash
set -euo pipefail
trap 'echo "ERROR on line $LINENO. Exit code: $?"; exit 1' ERR
# Stack name (first argument)
STACK_NAME=${1:-atlasnet_dev}

# SHARD image name (second argument)
SHARD_IMAGE_NAME=${2:-"shard:latest"}

# Path to the stack file
STACK_FILE="./docker-stack-dev.yml"

if ! command -v docker >/dev/null 2>&1; then
    echo "Error: docker CLI is not available."
    exit 1
fi

if ! docker info >/dev/null 2>&1; then
    echo "Error: docker daemon is not reachable (check /var/run/docker.sock mount/permissions)."
    exit 1
fi

SWARM_STATE="$(docker info --format '{{.Swarm.LocalNodeState}}' 2>/dev/null || echo inactive)"
if [ "$SWARM_STATE" != "active" ]; then
    echo "Docker Swarm is not active. Initializing single-node swarm..."
    if [ -n "${SWARM_ADVERTISE_ADDR:-}" ]; then
        docker swarm init --advertise-addr "${SWARM_ADVERTISE_ADDR}"
    else
        docker swarm init
    fi
fi

wait_for_stack_services() {
    local timeout="${ATLASNET_STACK_READY_TIMEOUT:-180}"
    local elapsed=0

    if ! [[ "$timeout" =~ ^[0-9]+$ ]] || [ "$timeout" -lt 1 ]; then
        timeout=180
    fi

    echo "Waiting for stack services to reach desired replicas (timeout: ${timeout}s)..."
    while [ "$elapsed" -lt "$timeout" ]; do
        mapfile -t service_rows < <(docker stack services --format '{{.Name}}\t{{.Replicas}}' "$STACK_NAME" 2>/dev/null || true)
        if [ "${#service_rows[@]}" -eq 0 ]; then
            sleep 1
            elapsed=$((elapsed + 1))
            continue
        fi

        local all_ready=1
        for row in "${service_rows[@]}"; do
            local replicas="${row#*$'\t'}"
            local current="${replicas%/*}"
            local desired="${replicas#*/}"

            if ! [[ "$current" =~ ^[0-9]+$ && "$desired" =~ ^[0-9]+$ ]]; then
                all_ready=0
                break
            fi
            if [ "$current" -lt "$desired" ]; then
                all_ready=0
                break
            fi
        done

        if [ "$all_ready" -eq 1 ]; then
            echo "All stack services reached desired replicas."
            docker stack services "$STACK_NAME" || true
            return 0
        fi

        sleep 1
        elapsed=$((elapsed + 1))
    done

    echo "Timed out waiting for stack services to reach desired replicas."
    docker stack services "$STACK_NAME" || true
    return 1
}

# Check if stack file exists
if [ ! -f "$STACK_FILE" ]; then
    echo "Error: $STACK_FILE not found."
    exit 1
fi

# Prepare a temporary stack file with SHARD_IMAGE_NAME replaced
TEMP_STACK_FILE=$(mktemp /tmp/docker-stack-XXXX.yml)

# Replace the macro SHARD_IMAGE_NAME in the YAML file
sed "s|SHARD_IMAGE_NAME|$SHARD_IMAGE_NAME|g" "$STACK_FILE" > "$TEMP_STACK_FILE"

# Check if the stack is already running
if docker stack ls --format '{{.Name}}' | grep -w "$STACK_NAME" > /dev/null; then
    echo "Forcefully removing existing stack '$STACK_NAME'..."

    # Kill all services in the stack
    for svc in $(docker stack services --format '{{.Name}}' "$STACK_NAME"); do
        echo "Removing service $svc..."
        docker service rm "$svc" 2>/dev/null
    done

    # Kill all containers in the stack (in case services linger)
    for ctr in $(docker ps -q --filter "label=com.docker.stack.namespace=$STACK_NAME"); do
        echo "Killing container $ctr..."
        docker kill "$ctr" 2>/dev/null
        docker rm -f "$ctr" 2>/dev/null
    done

    # Finally, remove the stack
    docker stack rm "$STACK_NAME" 2>/dev/null

    # Wait until the stack is gone
    echo "Waiting for services to terminate..."
    while docker service ls --format '{{.Name}}' | grep "$STACK_NAME" > /dev/null; do
        sleep 1
    done
fi
# Ensure the old stack network is fully gone before redeploy.
STACK_NETWORK_NAME="${STACK_NAME}_AtlasNet"
if docker network ls --format '{{.Name}}' | grep -Fx "$STACK_NETWORK_NAME" > /dev/null; then
    echo "Removing network $STACK_NETWORK_NAME"
    docker network rm "$STACK_NETWORK_NAME" > /dev/null 2>&1 || true

    echo "Waiting for network to terminate..."
    while docker network ls --format '{{.Name}}' | grep -Fx "$STACK_NETWORK_NAME" > /dev/null; do
        sleep 1
    done
fi

# Deploy the stack using the temporary file

echo "Deploying Docker stack '$STACK_NAME' with shard image '$SHARD_IMAGE_NAME'..."
set +e  # temporarily allow failure
docker stack deploy -c "$TEMP_STACK_FILE" "$STACK_NAME" --detach=true
DEPLOY_EXIT=$?
set -e
if [ $DEPLOY_EXIT -ne 0 ]; then
    echo "docker stack deploy failed with exit code $DEPLOY_EXIT"
    docker stack ps "$STACK_NAME"
    exit $DEPLOY_EXIT
fi

# Clean up
rm "$TEMP_STACK_FILE"

wait_for_stack_services

echo "Ensuring latency helper is running..."
bash "${PWD}/EnsureLatencyHelper.sh"

echo "Stack '$STACK_NAME' deployed successfully."
