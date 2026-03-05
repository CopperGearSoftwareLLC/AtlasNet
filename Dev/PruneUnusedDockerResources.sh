#!/usr/bin/env bash
set -euo pipefail

SKIP_PRUNE="${ATLASNET_SKIP_DOCKER_PRUNE:-0}"

if [[ "$SKIP_PRUNE" == "1" ]]; then
    echo "==> Skipping Docker prune (ATLASNET_SKIP_DOCKER_PRUNE=1)."
    exit 0
fi

require_cmd() {
    local cmd="$1"
    local hint="${2:-}"
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Error: '$cmd' is required but not installed." >&2
        if [[ -n "$hint" ]]; then
            echo "$hint" >&2
        fi
        exit 1
    fi
}

require_cmd docker "Hint: ensure Docker CLI is installed and /var/run/docker.sock is mounted."

if ! docker info >/dev/null 2>&1; then
    echo "Error: docker daemon is not reachable." >&2
    exit 1
fi

echo "==> Pruning unused Docker images..."
docker image prune --all --force

echo "==> Pruning unused Docker volumes..."
docker volume prune --force

echo "==> Docker prune complete."
