#!/usr/bin/env bash
set -euo pipefail

# Default platforms (used only if user wants to override)
DEFAULT_PLATFORMS="linux/amd64,linux/arm64"

# Default bake file location
BAKE_FILE="${BAKE_FILE:-$(dirname "$0")/Dockerfiles/docker-bake.json}"

# Builder name
BUILDER_NAME="atlasnet-builder"
BUILDKITD_CONFIG="${BUILDKITD_CONFIG:-$(dirname "$0")/buildkitd.toml}"

usage() {
    echo "Usage: $0 [-p platforms] [-f bake_file] [-c]"
    echo "  -p    Comma-separated platforms (default: use bake file settings)"
    echo "  -f    Path to docker-bake.json (default: $BAKE_FILE)"
    echo "  -c    Compatibility flag (accepted, no-op)"
    exit 1
}

PLATFORMS=""

while getopts "p:f:ch" opt; do
    case "$opt" in
        p) PLATFORMS="$OPTARG" ;;
        f) BAKE_FILE="$OPTARG" ;;
        c) ;;
        h) usage ;;
        *) usage ;;
    esac
done

# Determine progress style
PROGRESS=$([ -t 1 ] && echo "tty" || echo "plain")

echo "==> Using bake file: $BAKE_FILE"
[ -n "$PLATFORMS" ] && echo "==> Using platforms override: $PLATFORMS"

# Ensure buildx exists
if ! docker buildx version >/dev/null 2>&1; then
    echo "Docker buildx is required."
    exit 1
fi

create_builder() {
    echo "==> Creating buildx builder: $BUILDER_NAME"
    docker buildx create \
        --name "$BUILDER_NAME" \
        --driver docker-container \
        --driver-opt network=host \
        --buildkitd-config "$BUILDKITD_CONFIG" \
        --use
}

# Only create builder if missing or misconfigured
use_or_create_builder() {
    if ! docker buildx inspect "$BUILDER_NAME" >/dev/null 2>&1; then
        echo "==> Builder does not exist, creating..."
        create_builder

        #echo "==> Builder exists, checking configuration..."
        #NETWORK_OK=$(docker buildx inspect "$BUILDER_NAME" | grep -q 'network="host"' && echo "yes" || echo "no")
        #CONFIG_OK=$(docker buildx inspect "$BUILDER_NAME" | grep -q "File#$(basename "$BUILDKITD_CONFIG"):" && echo "yes" || echo "no")
        #
        #if [ "$NETWORK_OK" != "yes" ] || [ "$CONFIG_OK" != "yes" ]; then
        #    echo "==> Builder config drift detected, recreating builder..."
        #    docker buildx rm -f "$BUILDER_NAME" >/dev/null 2>&1 || true
        #    create_builder
        #else
        #    echo "==> Builder configuration is up to date."
        #    docker buildx use "$BUILDER_NAME"
        #fi
    fi
}

# Use builder or create if missing
use_or_create_builder

# Bootstrap builder (enables QEMU for cross-arch)
docker buildx inspect "$BUILDER_NAME" --bootstrap

echo "==> Building images in parallel with BuildKit..."

# Conditionally include platforms if specified
if [ -n "$PLATFORMS" ]; then
    docker buildx bake -f "$BAKE_FILE" --set "*.platform=$PLATFORMS" --load
else
    docker buildx bake -f "$BAKE_FILE" --load
fi

echo "==> Done."
