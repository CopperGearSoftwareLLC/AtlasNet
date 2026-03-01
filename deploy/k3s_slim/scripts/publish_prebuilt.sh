#!/usr/bin/env bash
set -euo pipefail

TEMPLATE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "${TEMPLATE_DIR}/../.." && pwd)"
ENV_FILE="${TEMPLATE_DIR}/.env"
BUILD_PUSH_SCRIPT="${REPO_ROOT}/Dev/BuildAndPushAtlasNetImages.sh"
STAGE_SCRIPT="${TEMPLATE_DIR}/scripts/stage_prebuilt.sh"
TAG_RECORD_FILE="${TEMPLATE_DIR}/config/image_tag"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

registry_from_repo() {
  local repo="$1"
  local first_segment="${repo%%/*}"
  if [[ "${first_segment}" == *.* || "${first_segment}" == *:* || "${first_segment}" == "localhost" ]]; then
    printf '%s' "${first_segment}"
    return
  fi
  printf '%s' "docker.io"
}

require_registry_login_for_push() {
  local registry="$1"
  local docker_config="${DOCKER_CONFIG:-${HOME}/.docker}/config.json"

  [[ -f "${docker_config}" ]] || die "Docker config not found at ${docker_config}. Run 'docker login ${registry}' first."

  # If a credential helper is configured, do not enforce file-based auth key checks.
  if grep -Eq '"credsStore"|"credHelpers"' "${docker_config}"; then
    return 0
  fi

  case "${registry}" in
    docker.io|index.docker.io|registry-1.docker.io)
      if ! grep -Eq '"(https://index\.docker\.io/v1/|index\.docker\.io|docker\.io)"' "${docker_config}"; then
        die "No Docker Hub credentials found. Run 'docker login docker.io -u <dockerhub-username>' and retry."
      fi
      ;;
    *)
      if ! grep -Fq "\"${registry}\"" "${docker_config}"; then
        die "No credentials found for '${registry}'. Run 'docker login ${registry}' and retry."
      fi
      ;;
  esac
}

declare -A OVERRIDES=()
capture_override() {
  local key="$1"
  if [[ -n "${!key+x}" ]]; then
    OVERRIDES["$key"]="${!key}"
  fi
}
restore_overrides() {
  local key
  for key in "${!OVERRIDES[@]}"; do
    printf -v "$key" '%s' "${OVERRIDES[$key]}"
    export "$key"
  done
}

for key in IMAGE_REPO IMAGE_TAG IMAGE_BUILD_PLATFORM IMAGE_PLATFORMS BUILDER_NAME WITH_LATEST_TAG PREBUILT_BAKE_FILE PREBUILT_IMAGE_PLATFORM PREBUILT_BUILD_DIR PREBUILT_WEB_NODE_PATH BUILD_DIR; do
  capture_override "$key"
done

if [[ -f "${ENV_FILE}" ]]; then
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
fi
restore_overrides

need_cmd bash
[[ -x "${BUILD_PUSH_SCRIPT}" ]] || die "Missing executable script: ${BUILD_PUSH_SCRIPT}"
[[ -x "${STAGE_SCRIPT}" ]] || die "Missing executable script: ${STAGE_SCRIPT}"

IMAGE_REPO="${IMAGE_REPO:-}"
IMAGE_TAG="${IMAGE_TAG:-}"
IMAGE_PLATFORMS="${IMAGE_PLATFORMS:-linux/amd64,linux/arm64}"
IMAGE_BUILD_PLATFORM="${IMAGE_BUILD_PLATFORM:-${IMAGE_PLATFORMS%%,*}}"
PREBUILT_IMAGE_PLATFORM="${PREBUILT_IMAGE_PLATFORM:-${IMAGE_BUILD_PLATFORM}}"
BUILDER_NAME="${BUILDER_NAME:-}"
WITH_LATEST_TAG="${WITH_LATEST_TAG:-false}"
PREBUILT_BAKE_FILE="${PREBUILT_BAKE_FILE:-docker/dockerfiles/docker-bake.copy.json}"
PREBUILT_BUILD_DIR="${PREBUILT_BUILD_DIR:-${BUILD_DIR:-}}"
PREBUILT_WEB_NODE_PATH="${PREBUILT_WEB_NODE_PATH:-}"

: "${IMAGE_REPO:?Set IMAGE_REPO in deploy/k3s_slim/.env}"
[[ -n "${PREBUILT_IMAGE_PLATFORM}" ]] || die "Unable to resolve PREBUILT_IMAGE_PLATFORM"
if [[ "${PREBUILT_IMAGE_PLATFORM}" == *,* ]]; then
  die "publish-prebuilt supports one platform at a time. Set PREBUILT_IMAGE_PLATFORM (example: linux/amd64)."
fi

REGISTRY_HOST="$(registry_from_repo "${IMAGE_REPO}")"
require_registry_login_for_push "${REGISTRY_HOST}"

TAG_SOURCE="env"
if [[ -z "${IMAGE_TAG}" ]]; then
  if git -C "${REPO_ROOT}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    IMAGE_TAG="$(git -C "${REPO_ROOT}" rev-parse --short HEAD 2>/dev/null || true)"
  fi
  if [[ -n "${IMAGE_TAG}" ]]; then
    TAG_SOURCE="git-sha"
  fi
fi
if [[ -z "${IMAGE_TAG}" ]]; then
  IMAGE_TAG="latest"
  TAG_SOURCE="fallback-latest"
fi

if [[ -n "${PREBUILT_BUILD_DIR}" ]]; then
  export PREBUILT_BUILD_DIR
fi
if [[ -n "${PREBUILT_WEB_NODE_PATH}" ]]; then
  export PREBUILT_WEB_NODE_PATH
fi

echo "Staging prebuilt artifacts from local build outputs..."
"${STAGE_SCRIPT}"

args=(
  --repo "${IMAGE_REPO}"
  --tag "${IMAGE_TAG}"
  --platforms "${PREBUILT_IMAGE_PLATFORM}"
  --bake-file "${PREBUILT_BAKE_FILE}"
  --skip-stage-build
)

if [[ -n "${BUILDER_NAME}" ]]; then
  args+=(--builder "${BUILDER_NAME}")
fi
if [[ "${WITH_LATEST_TAG}" == "true" ]]; then
  args+=(--with-latest)
fi

echo "Publishing AtlasNet images from prebuilt artifacts..."
echo "  repo: ${IMAGE_REPO}"
echo "  tag:  ${IMAGE_TAG} (${TAG_SOURCE})"
echo "  platform: ${PREBUILT_IMAGE_PLATFORM}"
echo "  bake: ${PREBUILT_BAKE_FILE}"

"${BUILD_PUSH_SCRIPT}" "${args[@]}"

mkdir -p "$(dirname "${TAG_RECORD_FILE}")"
printf '%s\n' "${IMAGE_TAG}" > "${TAG_RECORD_FILE}"
echo "Recorded image tag in ${TAG_RECORD_FILE}"
