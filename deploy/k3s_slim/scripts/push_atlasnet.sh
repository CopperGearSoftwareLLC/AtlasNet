#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "$ROOT_DIR/../.." && pwd)"
ENV_FILE="$ROOT_DIR/.env"
ATLASNET_DOCKERFILE="$REPO_ROOT/AtlasNet/docker/dockerfiles/BuildDockerfile"
ATLASNET_CONTEXT="$REPO_ROOT/AtlasNet"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

[[ -f "$ENV_FILE" ]] || die "Missing .env at $ENV_FILE"
# shellcheck disable=SC1090
source "$ENV_FILE"

need_cmd docker
docker info >/dev/null 2>&1 || die "Docker daemon is not reachable."
docker buildx version >/dev/null 2>&1 || die "Docker buildx is required."

: "${DOCKERHUB_NAMESPACE:=}"
: "${ATLASNET_IMAGE_TAG:=latest}"
: "${ATLASNET_MULTIARCH_PLATFORMS:=linux/amd64,linux/arm64}"
# If ATLASNET_MULTIARCH_BUILDER_NAME is empty, the current default buildx builder is used.
: "${ATLASNET_MULTIARCH_BUILDER_NAME:=}"
: "${DOCKERHUB_USERNAME:=}"
: "${DOCKERHUB_TOKEN:=}"

[[ -n "$DOCKERHUB_NAMESPACE" ]] || die "DOCKERHUB_NAMESPACE is required in .env"
[[ -f "$ATLASNET_DOCKERFILE" ]] || die "Missing Dockerfile: $ATLASNET_DOCKERFILE"

: "${ATLASNET_WATCHDOG_IMAGE:=${DOCKERHUB_NAMESPACE}/watchdog:${ATLASNET_IMAGE_TAG}}"
: "${ATLASNET_PROXY_IMAGE:=${DOCKERHUB_NAMESPACE}/proxy:${ATLASNET_IMAGE_TAG}}"
: "${ATLASNET_SHARD_IMAGE:=${DOCKERHUB_NAMESPACE}/shard:${ATLASNET_IMAGE_TAG}}"
: "${ATLASNET_CARTOGRAPH_IMAGE:=${DOCKERHUB_NAMESPACE}/cartograph:${ATLASNET_IMAGE_TAG}}"

ensure_buildx_builder() {
  if [[ -n "$ATLASNET_MULTIARCH_BUILDER_NAME" ]]; then
    # Use an explicitly configured builder; do NOT create a new one.
    if ! docker buildx inspect "$ATLASNET_MULTIARCH_BUILDER_NAME" >/dev/null 2>&1; then
      die "ATLASNET_MULTIARCH_BUILDER_NAME='$ATLASNET_MULTIARCH_BUILDER_NAME' does not exist. Run 'docker buildx ls' and either create it or unset the variable to use the current default builder."
    fi
    docker buildx use "$ATLASNET_MULTIARCH_BUILDER_NAME" >/dev/null
    docker buildx inspect "$ATLASNET_MULTIARCH_BUILDER_NAME" --bootstrap >/dev/null
  else
    # Reuse whatever default builder your dev environment is already using
    # (e.g. your shared buildx container with warm cache).
    docker buildx inspect >/dev/null 2>&1 || die "No default buildx builder configured. Run 'docker buildx create --use' once, or set ATLASNET_MULTIARCH_BUILDER_NAME in .env."
    docker buildx inspect --bootstrap >/dev/null
  fi
}

build_push_target() {
  local target="$1"
  local image_ref="$2"

  echo "Building + pushing $image_ref (target=$target, platforms=$ATLASNET_MULTIARCH_PLATFORMS)"
  docker buildx build \
    --platform "$ATLASNET_MULTIARCH_PLATFORMS" \
    -f "$ATLASNET_DOCKERFILE" \
    --target "$target" \
    -t "$image_ref" \
    --push \
    "$ATLASNET_CONTEXT"
}

if [[ -n "$DOCKERHUB_USERNAME" && -n "$DOCKERHUB_TOKEN" ]]; then
  echo "Logging into Docker Hub as $DOCKERHUB_USERNAME ..."
  printf '%s' "$DOCKERHUB_TOKEN" | docker login --username "$DOCKERHUB_USERNAME" --password-stdin
fi

ensure_buildx_builder

echo "Publishing AtlasNet multi-arch images to Docker Hub namespace '$DOCKERHUB_NAMESPACE' ..."
build_push_target "watchdog" "$ATLASNET_WATCHDOG_IMAGE"
build_push_target "proxy" "$ATLASNET_PROXY_IMAGE"
build_push_target "shard" "$ATLASNET_SHARD_IMAGE"
build_push_target "cartograph" "$ATLASNET_CARTOGRAPH_IMAGE"

echo
echo "Done. Published images:"
echo " - $ATLASNET_WATCHDOG_IMAGE"
echo " - $ATLASNET_PROXY_IMAGE"
echo " - $ATLASNET_SHARD_IMAGE"
echo " - $ATLASNET_CARTOGRAPH_IMAGE"
echo
echo "Next:"
echo " - deploy/k3s_slim: make atlasnet-deploy"
