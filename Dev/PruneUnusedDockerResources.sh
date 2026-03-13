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

run_prune_step() {
    local label="$1"
    shift

    local output=""
    local status=0

    echo "==> ${label}..."
    if ! output="$("$@" 2>&1)"; then
        status=$?
        echo "Error: ${label} failed." >&2
        if [[ -n "$output" ]]; then
            printf '%s\n' "$output" >&2
        fi
        exit "$status"
    fi

    if [[ -n "$output" ]]; then
        printf '%s\n' "$output"
    else
        echo "(no Docker output)"
    fi
}

run_prune_step "Pruning unused Docker images" docker image prune --all --force
run_prune_step "Pruning unused Docker volumes" docker volume prune --force

echo "==> Docker prune complete."
