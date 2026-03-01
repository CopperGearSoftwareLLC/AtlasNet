#!/usr/bin/env bash
set -euo pipefail

TEMPLATE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "${TEMPLATE_DIR}/../.." && pwd)"
ENV_FILE="${TEMPLATE_DIR}/.env"
BUILD_PUSH_SCRIPT="${REPO_ROOT}/Dev/BuildAndPushAtlasNetImages.sh"
TAG_RECORD_FILE="${TEMPLATE_DIR}/config/image_tag"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

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

for key in IMAGE_REPO IMAGE_TAG IMAGE_PLATFORMS IMAGE_BUILD_PLATFORM BUILD_DIR BAKE_FILE BUILDER_NAME WITH_LATEST_TAG; do
  capture_override "$key"
done

if [[ -f "${ENV_FILE}" ]]; then
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
fi
restore_overrides

need_cmd bash
[[ -x "${BUILD_PUSH_SCRIPT}" ]] || die "Missing executable script: ${BUILD_PUSH_SCRIPT}"

# Unprefixed names are primary.
IMAGE_REPO="${IMAGE_REPO:-}"
IMAGE_TAG="${IMAGE_TAG:-}"
IMAGE_PLATFORMS="${IMAGE_PLATFORMS:-linux/amd64,linux/arm64}"
IMAGE_BUILD_PLATFORM="${IMAGE_BUILD_PLATFORM:-}"
BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/build}"
BAKE_FILE="${BAKE_FILE:-}"
BUILDER_NAME="${BUILDER_NAME:-}"
WITH_LATEST_TAG="${WITH_LATEST_TAG:-false}"
TOOLCHAIN_FILE="${TOOLCHAIN_FILE:-${CMAKE_TOOLCHAIN_FILE:-}}"
SKIP_STAGE_BUILD="false"

if [[ "${BAKE_FILE}" == "docker/dockerfiles/docker-bake.json" || "${BAKE_FILE}" == */docker/dockerfiles/docker-bake.json ]]; then
  SKIP_STAGE_BUILD="true"
fi

: "${IMAGE_REPO:?Set IMAGE_REPO in deploy/k3s_slim/.env}"

cache_usable_for_repo() {
  local dir="$1"
  local cache_file="$dir/CMakeCache.txt"
  local repo_abs cache_home cache_home_abs toolchain_file

  [[ -f "${cache_file}" ]] || return 0
  repo_abs="$(readlink -f "${REPO_ROOT}" 2>/dev/null || printf '%s' "${REPO_ROOT}")"
  cache_home="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "${cache_file}" | tail -n 1)"
  [[ -n "${cache_home}" ]] || return 0
  cache_home_abs="$(readlink -f "${cache_home}" 2>/dev/null || printf '%s' "${cache_home}")"
  [[ "${cache_home_abs}" == "${repo_abs}" ]] || return 1

  toolchain_file="$(sed -n 's/^CMAKE_TOOLCHAIN_FILE:FILEPATH=//p' "${cache_file}" | tail -n 1)"
  if [[ -n "${toolchain_file}" && ! -f "${toolchain_file}" ]]; then
    return 1
  fi
  return 0
}

if [[ "${SKIP_STAGE_BUILD}" != "true" ]]; then
  if ! cache_usable_for_repo "${BUILD_DIR}"; then
    fallback_build_dir="${REPO_ROOT}/build-local"
    if cache_usable_for_repo "${fallback_build_dir}"; then
      echo "Build dir '${BUILD_DIR}' has an incompatible CMake cache; using '${fallback_build_dir}' instead."
      BUILD_DIR="${fallback_build_dir}"
    else
      die "Build dir '${BUILD_DIR}' has an incompatible CMake cache. Reconfigure with: cmake -S '${REPO_ROOT}' -B '${BUILD_DIR}'"
    fi
  fi
fi

build_files_present() {
  local dir="$1"
  local cache_file="$dir/CMakeCache.txt"
  local generator

  [[ -f "${cache_file}" ]] || return 1
  generator="$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "${cache_file}" | tail -n 1)"
  case "${generator}" in
    *Ninja*)
      [[ -f "${dir}/build.ninja" ]]
      ;;
    *Makefiles*)
      [[ -f "${dir}/Makefile" ]]
      ;;
    *)
      [[ -f "${dir}/build.ninja" || -f "${dir}/Makefile" ]]
      ;;
  esac
}

ensure_configured_build_dir() {
  local dir="$1"
  local toolchain_file=""
  local cmake_args

  if build_files_present "${dir}"; then
    return 0
  fi

  if [[ -n "${TOOLCHAIN_FILE}" && -f "${TOOLCHAIN_FILE}" ]]; then
    toolchain_file="${TOOLCHAIN_FILE}"
  elif [[ -n "${VCPKG_ROOT:-}" && -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]]; then
    toolchain_file="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
  elif [[ -f "${HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake" ]]; then
    toolchain_file="${HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake"
  elif [[ -f "/usr/local/vcpkg/scripts/buildsystems/vcpkg.cmake" ]]; then
    toolchain_file="/usr/local/vcpkg/scripts/buildsystems/vcpkg.cmake"
  elif [[ -f "/opt/vcpkg/scripts/buildsystems/vcpkg.cmake" ]]; then
    toolchain_file="/opt/vcpkg/scripts/buildsystems/vcpkg.cmake"
  fi

  echo "Build dir '${dir}' is missing generated build files; running cmake configure..."
  cmake_args=(-S "${REPO_ROOT}" -B "${dir}")
  if [[ -n "${toolchain_file}" ]]; then
    echo "Configuring with toolchain: ${toolchain_file}"
    cmake_args+=(-DCMAKE_TOOLCHAIN_FILE="${toolchain_file}")
  fi
  if ! cmake "${cmake_args[@]}"; then
    if [[ -z "${toolchain_file}" ]]; then
      die "CMake configure failed. Set TOOLCHAIN_FILE in .env (example: \$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake) and rerun."
    fi
    die "CMake configure failed using toolchain '${toolchain_file}'."
  fi
  build_files_present "${dir}" || die "Build dir '${dir}' is still missing build files after configure."
}

if [[ "${SKIP_STAGE_BUILD}" != "true" ]]; then
  ensure_configured_build_dir "${BUILD_DIR}"
fi

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

if [[ -z "${IMAGE_BUILD_PLATFORM}" ]]; then
  IMAGE_BUILD_PLATFORM="${IMAGE_PLATFORMS%%,*}"
fi
[[ -n "${IMAGE_BUILD_PLATFORM}" ]] || die "Unable to resolve IMAGE_BUILD_PLATFORM"

args=(
  --repo "${IMAGE_REPO}"
  --tag "${IMAGE_TAG}"
  --platforms "${IMAGE_BUILD_PLATFORM}"
  --build-dir "${BUILD_DIR}"
  --no-push
)

if [[ -n "${BAKE_FILE}" ]]; then
  args+=(--bake-file "${BAKE_FILE}")
fi
if [[ -n "${BUILDER_NAME}" ]]; then
  args+=(--builder "${BUILDER_NAME}")
fi
if [[ "${WITH_LATEST_TAG}" == "true" ]]; then
  args+=(--with-latest)
fi
if [[ "${SKIP_STAGE_BUILD}" == "true" ]]; then
  args+=(--skip-stage-build)
fi

echo "Building local AtlasNet images via ${BUILD_PUSH_SCRIPT}"
echo "  repo: ${IMAGE_REPO}"
echo "  tag:  ${IMAGE_TAG} (${TAG_SOURCE})"
echo "  platform: ${IMAGE_BUILD_PLATFORM}"
if [[ -n "${BAKE_FILE}" ]]; then
  echo "  bake: ${BAKE_FILE}"
fi
if [[ "${SKIP_STAGE_BUILD}" == "true" ]]; then
  echo "  stage: skipped (BuildDockerfile flow)"
fi

"${BUILD_PUSH_SCRIPT}" "${args[@]}"

mkdir -p "$(dirname "${TAG_RECORD_FILE}")"
printf '%s\n' "${IMAGE_TAG}" > "${TAG_RECORD_FILE}"
echo "Recorded image tag in ${TAG_RECORD_FILE}"
