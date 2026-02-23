#!/usr/bin/env bash
set -euo pipefail

# Default platforms (used only if user wants to override)
DEFAULT_PLATFORMS="linux/amd64,linux/arm64"

# Default bake file location
BAKE_FILE="${BAKE_FILE:-$(dirname "$0")/Dockerfiles/docker-bake.json}"

# Builder name
BUILDER_NAME="atlasnet-builder"

usage() {
    echo "Usage: $0 [-p platforms] [-f bake_file]"
    echo "  -p    Comma-separated platforms (default: use bake file settings)"
    echo "  -f    Path to docker-bake.json (default: $BAKE_FILE)"
    exit 1
}

PLATFORMS=""

while getopts "p:f:h" opt; do
    case "$opt" in
        p) PLATFORMS="$OPTARG" ;;
        f) BAKE_FILE="$OPTARG" ;;
        h) usage ;;
        *) usage ;;
    esac
done

# Determine progress style
if [ -t 1 ]; then
    PROGRESS="tty"
else
    PROGRESS="plain"
fi

echo "==> Using bake file: $BAKE_FILE"
[ -n "$PLATFORMS" ] && echo "==> Using platforms override: $PLATFORMS"

# Ensure buildx exists
if ! docker buildx version >/dev/null 2>&1; then
    echo "Docker buildx is required."
    exit 1
fi

# Create builder if it doesn't exist
if ! docker buildx inspect "$BUILDER_NAME" >/dev/null 2>&1; then
    echo "==> Creating buildx builder: $BUILDER_NAME"
    docker buildx create \
        --name "$BUILDER_NAME" \
        --driver docker-container \
        --use
else
    docker buildx use "$BUILDER_NAME"
fi

# Bootstrap builder (enables QEMU for cross-arch)
docker buildx inspect --bootstrap

echo "==> Building images in parallel with BuildKit..."

# Conditionally include platforms if specified
if [ -n "$PLATFORMS" ]; then
    docker buildx bake -f "$BAKE_FILE" --set "*.platform=$PLATFORMS" --load
else
    docker buildx bake -f "$BAKE_FILE" --load
fi

echo "==> Done."
