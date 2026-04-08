#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
source_env

need_cmd docker
docker buildx version >/dev/null 2>&1 || die "Docker buildx is required."

: "${DOCKERHUB_NAMESPACE:?DOCKERHUB_NAMESPACE is required in .env}"
: "${ATLASNET_IMAGE_TAG:?ATLASNET_IMAGE_TAG is required in .env}"
: "${ATLASNET_IMAGE_TAG_AMD64:?ATLASNET_IMAGE_TAG_AMD64 is required in .env}"
: "${ATLASNET_IMAGE_TAG_ARM64:?ATLASNET_IMAGE_TAG_ARM64 is required in .env}"

require_pinned_tag "ATLASNET_IMAGE_TAG" "$ATLASNET_IMAGE_TAG"
require_pinned_tag "ATLASNET_IMAGE_TAG_AMD64" "$ATLASNET_IMAGE_TAG_AMD64"
require_pinned_tag "ATLASNET_IMAGE_TAG_ARM64" "$ATLASNET_IMAGE_TAG_ARM64"

merge_one() {
  local name="$1"
  local target="${DOCKERHUB_NAMESPACE}/${name}:${ATLASNET_IMAGE_TAG}"
  local amd64_ref="${DOCKERHUB_NAMESPACE}/${name}:${ATLASNET_IMAGE_TAG_AMD64}"
  local arm64_ref="${DOCKERHUB_NAMESPACE}/${name}:${ATLASNET_IMAGE_TAG_ARM64}"

  echo "==> Merging '$target'"
  if ! docker buildx imagetools create -t "$target" "$amd64_ref" "$arm64_ref"; then
    echo "WARN: failed to create multi-arch manifest for $target" >&2
    return 1
  fi
}

failed=0
merge_one "watchdog" || failed=1
merge_one "proxy" || failed=1
merge_one "sandbox-server" || failed=1
merge_one "cartograph" || failed=1

echo
if [[ "$failed" -eq 0 ]]; then
  echo "Multi-arch deployment tags created successfully."
else
  echo "One or more manifest merges failed. Offline bundling can still use the per-arch source tags directly."
fi
