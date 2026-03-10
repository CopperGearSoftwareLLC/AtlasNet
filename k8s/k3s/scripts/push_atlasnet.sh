#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "$ROOT_DIR/../.." && pwd)"
ENV_FILE="$ROOT_DIR/.env"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

[[ -f "$ENV_FILE" ]] || die "Missing .env at $ENV_FILE"
# shellcheck disable=SC1090
source "$ENV_FILE"

need_cmd docker
docker info >/dev/null 2>&1 || die "Docker daemon is not reachable."

: "${DOCKERHUB_NAMESPACE:=}"
: "${ATLASNET_IMAGE_TAG:=latest}"
: "${ATLASNET_IMAGE_TAG_AMD64:=latest-amd64}"
: "${ATLASNET_IMAGE_TAG_ARM64:=latest-arm64}"

[[ -n "$DOCKERHUB_NAMESPACE" ]] || die "DOCKERHUB_NAMESPACE is required in .env"

# Automatically select an arch-specific tag (amd64 vs arm64) based on host architecture,
# while still allowing explicit ATLASNET_*_IMAGE overrides in .env to take precedence.
arch="$(uname -m || echo unknown)"
case "$arch" in
  x86_64|amd64)
    ARCH_TAG="$ATLASNET_IMAGE_TAG_AMD64"
    ;;
  aarch64|arm64)
    ARCH_TAG="$ATLASNET_IMAGE_TAG_ARM64"
    ;;
  *)
    echo "WARNING: Unknown architecture '${arch}', falling back to default tag '${ATLASNET_IMAGE_TAG}'." >&2
    ARCH_TAG="$ATLASNET_IMAGE_TAG"
    ;;
esac

: "${ATLASNET_WATCHDOG_IMAGE:=${DOCKERHUB_NAMESPACE}/watchdog:${ARCH_TAG}}"
: "${ATLASNET_PROXY_IMAGE:=${DOCKERHUB_NAMESPACE}/proxy:${ARCH_TAG}}"
: "${ATLASNET_CARTOGRAPH_IMAGE:=${DOCKERHUB_NAMESPACE}/cartograph:${ARCH_TAG}}"
: "${ATLASNET_SANDBOX_SERVER_IMAGE:=${DOCKERHUB_NAMESPACE}/sandbox-server:${ARCH_TAG}}"

tag_and_push() {
  local local_image="$1"   # e.g. watchdog:latest
  local target_ref="$2"    # e.g. dannys0n/watchdog:latest

  echo "==> Preparing to push $local_image as $target_ref"

  if ! docker image inspect "$local_image" >/dev/null 2>&1; then
    die "Local image '$local_image' not found. Build it first (e.g. via 'cmake --build build --target sandbox_atlasnet_run')."
  fi

  docker tag "$local_image" "$target_ref"
  docker push "$target_ref"
}

echo "Publishing AtlasNet runtime images to Docker Hub namespace '$DOCKERHUB_NAMESPACE' using existing local images..."
echo "NOTE: This assumes you've already built local images (e.g. via 'sandbox_atlasnet_run')."

tag_and_push "watchdog:latest"   "$ATLASNET_WATCHDOG_IMAGE"
tag_and_push "proxy:latest"      "$ATLASNET_PROXY_IMAGE"
tag_and_push "sandbox-server:latest" "$ATLASNET_SANDBOX_SERVER_IMAGE"
tag_and_push "cartograph:latest" "$ATLASNET_CARTOGRAPH_IMAGE"

echo
echo "Done. Published images:"
echo " - $ATLASNET_WATCHDOG_IMAGE"
echo " - $ATLASNET_PROXY_IMAGE"
echo " - $ATLASNET_SANDBOX_SERVER_IMAGE"
echo " - $ATLASNET_CARTOGRAPH_IMAGE"
echo
echo "Next:"
echo " - k8s/k3s: make atlasnet-deploy"
