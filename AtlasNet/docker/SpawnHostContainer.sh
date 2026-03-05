#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "Usage: $0 <container-name> <image> [docker-run-options]"
    exit 1
fi

CONTAINER_NAME="$1"
IMAGE="$2"
RUN_ARGS_RAW="${3:-}"

if ! command -v docker >/dev/null 2>&1; then
    echo "Docker CLI is not available in PATH."
    exit 1
fi

echo "==> DOCKER_HOST=${DOCKER_HOST:-unix:///var/run/docker.sock}"
if ! docker info >/dev/null 2>&1; then
    echo "Docker daemon is not reachable. Mount the host Docker socket into this environment."
    exit 1
fi

if docker ps -a --format '{{.Names}}' | grep -Fxq "$CONTAINER_NAME"; then
    echo "==> Replacing existing container: ${CONTAINER_NAME}"
    docker rm -f "$CONTAINER_NAME" >/dev/null
fi

RUN_ARGS=()
if [[ -n "$RUN_ARGS_RAW" ]]; then
    read -r -a RUN_ARGS <<< "$RUN_ARGS_RAW"
fi

echo "==> Starting container '${CONTAINER_NAME}' from image '${IMAGE}'"
docker run -d --name "$CONTAINER_NAME" "${RUN_ARGS[@]}" "$IMAGE"

echo "==> Container started:"
docker ps --filter "name=^/${CONTAINER_NAME}$"
