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
docker buildx version >/dev/null 2>&1 || die "Docker buildx is required (docker buildx plugin)."

: "${DOCKERHUB_NAMESPACE:=}"
: "${ATLASNET_IMAGE_TAG:=latest}"
: "${ATLASNET_IMAGE_TAG_AMD64:=latest-amd64}"
: "${ATLASNET_IMAGE_TAG_ARM64:=latest-arm64}"

[[ -n "$DOCKERHUB_NAMESPACE" ]] || die "DOCKERHUB_NAMESPACE is required in .env"
[[ -n "$ATLASNET_IMAGE_TAG" ]] || die "ATLASNET_IMAGE_TAG is required in .env"
[[ -n "$ATLASNET_IMAGE_TAG_AMD64" ]] || die "ATLASNET_IMAGE_TAG_AMD64 is required in .env"
[[ -n "$ATLASNET_IMAGE_TAG_ARM64" ]] || die "ATLASNET_IMAGE_TAG_ARM64 is required in .env"

echo "Merging per-arch images into multi-arch manifests for namespace '$DOCKERHUB_NAMESPACE':"
echo "  deploy tag:            $ATLASNET_IMAGE_TAG"
echo "  amd64 source tag:      $ATLASNET_IMAGE_TAG_AMD64"
echo "  arm64 source tag:      $ATLASNET_IMAGE_TAG_ARM64"
echo

merge_one() {
  local name="$1"
  local target="${DOCKERHUB_NAMESPACE}/${name}:${ATLASNET_IMAGE_TAG}"
  local amd64_ref="${DOCKERHUB_NAMESPACE}/${name}:${ATLASNET_IMAGE_TAG_AMD64}"
  local arm64_ref="${DOCKERHUB_NAMESPACE}/${name}:${ATLASNET_IMAGE_TAG_ARM64}"

  echo "==> Creating multi-arch manifest for '$target' from:"
  echo "      - $amd64_ref"
  echo "      - $arm64_ref"

  docker buildx imagetools create \
    -t "$target" \
    "$amd64_ref" \
    "$arm64_ref"
}

merge_one "watchdog"
merge_one "proxy"
merge_one "shard"
merge_one "cartograph"

echo
echo "Done. Multi-arch manifests created for:"
echo "  - ${DOCKERHUB_NAMESPACE}/watchdog:${ATLASNET_IMAGE_TAG}"
echo "  - ${DOCKERHUB_NAMESPACE}/proxy:${ATLASNET_IMAGE_TAG}"
echo "  - ${DOCKERHUB_NAMESPACE}/shard:${ATLASNET_IMAGE_TAG}"
echo "  - ${DOCKERHUB_NAMESPACE}/cartograph:${ATLASNET_IMAGE_TAG}"
*** End Patch```} ***!
