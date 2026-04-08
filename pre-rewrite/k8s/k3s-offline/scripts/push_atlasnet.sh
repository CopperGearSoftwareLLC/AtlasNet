#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
source_env

need_cmd docker
docker info >/dev/null 2>&1 || die "Docker daemon is not reachable."

: "${DOCKERHUB_NAMESPACE:?DOCKERHUB_NAMESPACE is required in .env}"
: "${ATLASNET_IMAGE_TAG_AMD64:?ATLASNET_IMAGE_TAG_AMD64 is required in .env}"
: "${ATLASNET_IMAGE_TAG_ARM64:?ATLASNET_IMAGE_TAG_ARM64 is required in .env}"

arch="$(normalize_arch "$(uname -m || echo unknown)")"
case "$arch" in
  amd64) arch_tag="$ATLASNET_IMAGE_TAG_AMD64" ;;
  arm64) arch_tag="$ATLASNET_IMAGE_TAG_ARM64" ;;
esac

: "${ATLASNET_WATCHDOG_IMAGE:=${DOCKERHUB_NAMESPACE}/watchdog:${arch_tag}}"
: "${ATLASNET_PROXY_IMAGE:=${DOCKERHUB_NAMESPACE}/proxy:${arch_tag}}"
: "${ATLASNET_SANDBOX_SERVER_IMAGE:=${DOCKERHUB_NAMESPACE}/sandbox-server:${arch_tag}}"
: "${ATLASNET_CARTOGRAPH_IMAGE:=${DOCKERHUB_NAMESPACE}/cartograph:${arch_tag}}"

tag_and_push() {
  local local_image="$1"
  local target_ref="$2"

  if ! docker image inspect "$local_image" >/dev/null 2>&1; then
    die "Local image '$local_image' not found. Build it first before running make atlasnet-push."
  fi

  echo "==> Pushing $local_image as $target_ref"
  docker tag "$local_image" "$target_ref"
  docker push "$target_ref"
}

require_pinned_image "ATLASNET_WATCHDOG_IMAGE" "$ATLASNET_WATCHDOG_IMAGE"
require_pinned_image "ATLASNET_PROXY_IMAGE" "$ATLASNET_PROXY_IMAGE"
require_pinned_image "ATLASNET_SANDBOX_SERVER_IMAGE" "$ATLASNET_SANDBOX_SERVER_IMAGE"
require_pinned_image "ATLASNET_CARTOGRAPH_IMAGE" "$ATLASNET_CARTOGRAPH_IMAGE"

echo "Publishing AtlasNet runtime images for architecture '$arch' ..."
tag_and_push "watchdog:latest" "$ATLASNET_WATCHDOG_IMAGE"
tag_and_push "proxy:latest" "$ATLASNET_PROXY_IMAGE"
tag_and_push "sandbox-server:latest" "$ATLASNET_SANDBOX_SERVER_IMAGE"
tag_and_push "cartograph:latest" "$ATLASNET_CARTOGRAPH_IMAGE"

echo
echo "Done. Published:"
echo " - $ATLASNET_WATCHDOG_IMAGE"
echo " - $ATLASNET_PROXY_IMAGE"
echo " - $ATLASNET_SANDBOX_SERVER_IMAGE"
echo " - $ATLASNET_CARTOGRAPH_IMAGE"
