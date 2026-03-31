#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

BAKE_FILE="$ROOT_DIR/dev/docker/Production-bake.json"
DOCKERFILE_PATH="$ROOT_DIR/dev/docker/Production.DockerFile"
BUILDER="${BUILDER:-atlasnet-builder}"

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <arch> [arch ...]" >&2
  echo "Example: $0 amd64 arm64" >&2
  exit 1
fi

if [[ ! -f "$BAKE_FILE" ]]; then
  echo "Missing bake file: $BAKE_FILE" >&2
  exit 1
fi

if [[ ! -f "$DOCKERFILE_PATH" ]]; then
  echo "Missing Dockerfile: $DOCKERFILE_PATH" >&2
  exit 1
fi

PLATFORMS=()
for arch in "$@"; do
  case "$arch" in
    amd64|arm64|arm/v7|arm/v6|386)
      PLATFORMS+=("linux/$arch")
      ;;
    linux/*)
      PLATFORMS+=("$arch")
      ;;
    *)
      echo "Unsupported arch: $arch" >&2
      echo "Use values like: amd64, arm64, arm/v7, 386, or full linux/<arch>" >&2
      exit 1
      ;;
  esac
done

PLATFORM_CSV="$(IFS=,; echo "${PLATFORMS[*]}")"

if ! docker buildx inspect "$BUILDER" >/dev/null 2>&1; then
  docker buildx create --name "$BUILDER" --use
else
  docker buildx use "$BUILDER" >/dev/null
fi

docker buildx inspect --bootstrap >/dev/null

docker buildx bake \
  --file "$BAKE_FILE" \
  --set "*.dockerfile=$DOCKERFILE_PATH" \
  --set "*.platform=$PLATFORM_CSV" \
  --load