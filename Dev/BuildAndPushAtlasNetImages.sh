#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ATLASNET_ROOT="${ROOT_DIR}/AtlasNet"

IMAGE_REPO="${ATLASNET_IMAGE_REPO:-}"
IMAGE_TAG="${ATLASNET_IMAGE_TAG:-}"
PLATFORMS="${ATLASNET_IMAGE_PLATFORMS:-linux/amd64,linux/arm64}"
BUILD_DIR="${ATLASNET_BUILD_DIR:-${ROOT_DIR}/build}"
BAKE_FILE="${ATLASNET_BAKE_FILE:-${ATLASNET_ROOT}/docker/dockerfiles/docker-bake.copy.json}"
BUILDER_NAME="${ATLASNET_BUILDER_NAME:-atlasnet-builder}"
SKIP_STAGE_BUILD=0
WITH_LATEST_TAG=0
PUSH_IMAGES=1

usage() {
    cat <<'EOF'
Usage:
  Dev/BuildAndPushAtlasNetImages.sh --repo <registry/repo> [options]

Required:
  --repo <value>          Registry repo prefix (example: ghcr.io/acme/atlasnet)

Options:
  --tag <value>           Image tag (default: short git SHA, else "latest")
  --platforms <value>     Comma-separated platforms (default: linux/amd64,linux/arm64)
  --build-dir <path>      CMake build dir for staging target (default: ./build)
  --bake-file <path>      Docker bake file (default: AtlasNet/docker/dockerfiles/docker-bake.copy.json)
  --builder <name>        Docker buildx builder name (default: atlasnet-builder)
  --no-push               Build images locally (single platform) without publishing
  --skip-stage-build      Skip `AtlasnetDockerBuild_Fast_Stage`
  --with-latest           Push an additional :latest tag for each image
  -h, --help              Show this message

Notes:
  - Run `docker login <registry>` first if your registry requires auth.
  - This script publishes multi-arch images suitable for mixed amd64/arm64 k3s clusters.
EOF
}

require_cmd() {
    local cmd="$1"
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Error: '${cmd}' is required but not installed." >&2
        exit 1
    fi
}

while (($# > 0)); do
    case "$1" in
        --repo)
            IMAGE_REPO="${2:-}"
            shift 2
            ;;
        --tag)
            IMAGE_TAG="${2:-}"
            shift 2
            ;;
        --platforms)
            PLATFORMS="${2:-}"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="${2:-}"
            shift 2
            ;;
        --bake-file)
            BAKE_FILE="${2:-}"
            shift 2
            ;;
        --builder)
            BUILDER_NAME="${2:-}"
            shift 2
            ;;
        --no-push)
            PUSH_IMAGES=0
            shift
            ;;
        --skip-stage-build)
            SKIP_STAGE_BUILD=1
            shift
            ;;
        --with-latest)
            WITH_LATEST_TAG=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Error: unknown option '$1'" >&2
            usage
            exit 1
            ;;
    esac
done

if [[ -z "${IMAGE_REPO}" ]]; then
    echo "Error: --repo is required." >&2
    usage
    exit 1
fi

if [[ -z "${IMAGE_TAG}" ]]; then
    if git -C "${ROOT_DIR}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        IMAGE_TAG="$(git -C "${ROOT_DIR}" rev-parse --short HEAD 2>/dev/null || true)"
    fi
    IMAGE_TAG="${IMAGE_TAG:-latest}"
fi

if [[ "${BAKE_FILE}" != /* ]]; then
    if [[ -f "${ATLASNET_ROOT}/${BAKE_FILE}" ]]; then
        BAKE_FILE="${ATLASNET_ROOT}/${BAKE_FILE}"
    else
        BAKE_FILE="${ROOT_DIR}/${BAKE_FILE}"
    fi
fi

if [[ ! -f "${BAKE_FILE}" ]]; then
    echo "Error: bake file not found at '${BAKE_FILE}'." >&2
    exit 1
fi

require_cmd docker
require_cmd cmake

if ! docker info >/dev/null 2>&1; then
    echo "Error: docker daemon is not reachable." >&2
    exit 1
fi

if ! docker buildx version >/dev/null 2>&1; then
    echo "Error: docker buildx is required." >&2
    exit 1
fi

if ((SKIP_STAGE_BUILD == 0)); then
    if [[ ! -d "${BUILD_DIR}" ]]; then
        echo "Error: build dir '${BUILD_DIR}' does not exist." >&2
        echo "Hint: configure the project first, e.g. cmake -S . -B build" >&2
        exit 1
    fi
    echo "==> Staging AtlasNet artifacts via CMake target AtlasnetDockerBuild_Fast_Stage..."
    cmake --build "${BUILD_DIR}" --target AtlasnetDockerBuild_Fast_Stage
fi

if ! docker buildx inspect "${BUILDER_NAME}" >/dev/null 2>&1; then
    echo "==> Creating buildx builder '${BUILDER_NAME}'..."
    docker buildx create --name "${BUILDER_NAME}" --driver docker-container --use >/dev/null
else
    docker buildx use "${BUILDER_NAME}" >/dev/null
fi
docker buildx inspect --bootstrap >/dev/null

if ((PUSH_IMAGES == 0)) && [[ "${PLATFORMS}" == *,* ]]; then
    echo "Error: --no-push supports a single platform. Current value: ${PLATFORMS}" >&2
    echo "Hint: pass one platform, e.g. --platforms linux/amd64" >&2
    exit 1
fi

tags_for() {
    local image_name="$1"
    local primary="${IMAGE_REPO}/${image_name}:${IMAGE_TAG}"
    if ((WITH_LATEST_TAG == 1)); then
        printf '%s,%s/%s:latest' "${primary}" "${IMAGE_REPO}" "${image_name}"
        return
    fi
    printf '%s' "${primary}"
}

ATLASNETSDK_TAGS="$(tags_for atlasnetsdk)"
WATCHDOG_TAGS="$(tags_for watchdog)"
PROXY_TAGS="$(tags_for proxy)"
SHARD_TAGS="$(tags_for shard)"
CARTOGRAPH_TAGS="$(tags_for cartograph)"

if ((PUSH_IMAGES == 1)); then
    echo "==> Publishing AtlasNet images"
else
    echo "==> Building AtlasNet images (no push)"
fi
echo "    repo:      ${IMAGE_REPO}"
echo "    tag:       ${IMAGE_TAG}"
echo "    platforms: ${PLATFORMS}"

load_setting="true"
push_setting="false"
if ((PUSH_IMAGES == 1)); then
    load_setting="false"
    push_setting="true"
fi

pushd "${ATLASNET_ROOT}" >/dev/null
docker buildx bake \
    -f "${BAKE_FILE}" \
    --set "*.platform=${PLATFORMS}" \
    --set "*.load=${load_setting}" \
    --set "*.push=${push_setting}" \
    --set "atlasnetsdk.tags=${ATLASNETSDK_TAGS}" \
    --set "watchdog.tags=${WATCHDOG_TAGS}" \
    --set "proxy.tags=${PROXY_TAGS}" \
    --set "shard.tags=${SHARD_TAGS}" \
    --set "cartograph.tags=${CARTOGRAPH_TAGS}"
popd >/dev/null

echo
if ((PUSH_IMAGES == 1)); then
    echo "Published image refs:"
else
    echo "Built local image refs:"
fi
echo "  - ${IMAGE_REPO}/watchdog:${IMAGE_TAG}"
echo "  - ${IMAGE_REPO}/proxy:${IMAGE_TAG}"
echo "  - ${IMAGE_REPO}/shard:${IMAGE_TAG}"
echo "  - ${IMAGE_REPO}/cartograph:${IMAGE_TAG}"
if ((WITH_LATEST_TAG == 1)); then
    echo "  - ${IMAGE_REPO}/*:latest"
fi
