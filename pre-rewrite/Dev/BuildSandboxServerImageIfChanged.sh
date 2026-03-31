#!/usr/bin/env bash
set -euo pipefail

usage() {
    echo "Usage: $0 <sandbox_root> [image_name]" >&2
    exit 1
}

SANDBOX_ROOT="${1:-}"
IMAGE_NAME="${2:-sandbox-server:latest}"
STATE_FILE_OVERRIDE="${ATLASNET_SANDBOX_IMAGE_STATE_FILE:-}"

[[ -z "$SANDBOX_ROOT" ]] && usage

DOCKERFILE="${SANDBOX_ROOT}/SandboxServer.dockerfile"
STAGED_BINARY="${SANDBOX_ROOT}/.stage/SandboxServer"

sha256_stdin() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 | awk '{print $1}'
    else
        echo "Error: sha256sum (or shasum) is required." >&2
        exit 1
    fi
}

hash_file() {
    local file="$1"
    if [[ ! -f "$file" ]]; then
        printf 'missing'
        return
    fi
    <"$file" sha256_stdin
}

composite_hash() {
    printf '%s\n' "$@" | sha256_stdin
}

project_state_file() {
    local key
    key="$(printf '%s' "$SANDBOX_ROOT" | sha256_stdin | cut -c1-16)"
    printf '/tmp/atlasnet-sandbox-image-state-%s.txt' "$key"
}

if [[ ! -f "$DOCKERFILE" ]]; then
    echo "Error: sandbox Dockerfile not found: $DOCKERFILE" >&2
    exit 1
fi

if [[ ! -f "$STAGED_BINARY" ]]; then
    echo "Error: staged sandbox server binary not found: $STAGED_BINARY" >&2
    exit 1
fi

STATE_FILE="${STATE_FILE_OVERRIDE:-$(project_state_file)}"
PREV_HASH=""
if [[ -f "$STATE_FILE" ]]; then
    PREV_HASH="$(cat "$STATE_FILE" 2>/dev/null || true)"
fi

INPUT_HASH="$(
    composite_hash \
        "$(hash_file "$DOCKERFILE")" \
        "$(hash_file "$STAGED_BINARY")" \
        "$IMAGE_NAME"
)"

if [[ -n "$PREV_HASH" ]] \
    && [[ "$PREV_HASH" == "$INPUT_HASH" ]] \
    && docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
    echo "==> Sandbox image '$IMAGE_NAME' unchanged; skipping rebuild."
    exit 0
fi

echo "==> Building sandbox image '$IMAGE_NAME'..."
docker build \
    --network=host \
    -f "$DOCKERFILE" \
    -t "$IMAGE_NAME" \
    "$SANDBOX_ROOT"

mkdir -p "$(dirname "$STATE_FILE")"
printf '%s\n' "$INPUT_HASH" >"$STATE_FILE"

