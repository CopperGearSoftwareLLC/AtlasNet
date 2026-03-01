#!/usr/bin/env bash
set -euo pipefail

TEMPLATE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "${TEMPLATE_DIR}/../.." && pwd)"
ENV_FILE="${TEMPLATE_DIR}/.env"
ATLASNET_ROOT="${REPO_ROOT}/AtlasNet"
STAGE_DIR="${ATLASNET_ROOT}/.stage"

die() { echo "ERROR: $*" >&2; exit 1; }

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

for key in PREBUILT_BUILD_DIR BUILD_DIR PREBUILT_WEB_NODE_PATH; do
  capture_override "$key"
done

if [[ -f "${ENV_FILE}" ]]; then
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
fi
restore_overrides

PREBUILT_BUILD_DIR="${PREBUILT_BUILD_DIR:-${BUILD_DIR:-${REPO_ROOT}/build}}"
PREBUILT_WEB_NODE_PATH="${PREBUILT_WEB_NODE_PATH:-}"

find_first_file() {
  local p
  for p in "$@"; do
    if [[ -f "$p" ]]; then
      printf '%s' "$p"
      return 0
    fi
  done
  return 1
}

WATCHDOG_BIN="$(find_first_file \
  "${PREBUILT_BUILD_DIR}/AtlasNet/runtime/watchdog/watchdog" \
  "${PREBUILT_BUILD_DIR}/AtlasNet/runtime/watchdog/Debug/watchdog" \
  "${PREBUILT_BUILD_DIR}/AtlasNet/runtime/watchdog/Release/watchdog" \
  "${PREBUILT_BUILD_DIR}/runtime/watchdog/watchdog")" || \
  die "watchdog binary not found under ${PREBUILT_BUILD_DIR}. Build runtime targets first in VSCode/CMake."

SHARD_BIN="$(find_first_file \
  "${PREBUILT_BUILD_DIR}/AtlasNet/runtime/shard/shard" \
  "${PREBUILT_BUILD_DIR}/AtlasNet/runtime/shard/Debug/shard" \
  "${PREBUILT_BUILD_DIR}/AtlasNet/runtime/shard/Release/shard" \
  "${PREBUILT_BUILD_DIR}/runtime/shard/shard")" || \
  die "shard binary not found under ${PREBUILT_BUILD_DIR}. Build runtime targets first in VSCode/CMake."

PROXY_BIN="$(find_first_file \
  "${PREBUILT_BUILD_DIR}/AtlasNet/runtime/proxy/proxy" \
  "${PREBUILT_BUILD_DIR}/AtlasNet/runtime/proxy/Debug/proxy" \
  "${PREBUILT_BUILD_DIR}/AtlasNet/runtime/proxy/Release/proxy" \
  "${PREBUILT_BUILD_DIR}/runtime/proxy/proxy")" || \
  die "proxy binary not found under ${PREBUILT_BUILD_DIR}. Build runtime targets first in VSCode/CMake."

if [[ -z "${PREBUILT_WEB_NODE_PATH}" ]]; then
  PREBUILT_WEB_NODE_PATH="$(find_first_file \
    "${PREBUILT_BUILD_DIR}/AtlasNet/js/Web.node" \
    "${PREBUILT_BUILD_DIR}/AtlasNet/js/Debug/Web.node" \
    "${PREBUILT_BUILD_DIR}/AtlasNet/js/Release/Web.node" \
    "${PREBUILT_BUILD_DIR}/js/Web.node" \
    "${PREBUILT_BUILD_DIR}/js/Debug/Web.node" \
    "${PREBUILT_BUILD_DIR}/js/Release/Web.node")" || \
    die "Web.node not found under ${PREBUILT_BUILD_DIR}. Build Web_swig/native web module first in VSCode/CMake."
else
  [[ -f "${PREBUILT_WEB_NODE_PATH}" ]] || die "PREBUILT_WEB_NODE_PATH does not exist: ${PREBUILT_WEB_NODE_PATH}"
fi

CARTOGRAPH_DIR="${ATLASNET_ROOT}/runtime/cartograph"
[[ -d "${CARTOGRAPH_DIR}" ]] || die "Missing cartograph runtime directory: ${CARTOGRAPH_DIR}"

mkdir -p "${STAGE_DIR}"
rm -f "${STAGE_DIR}/watchdog" "${STAGE_DIR}/shard" "${STAGE_DIR}/proxy" "${STAGE_DIR}/Web.node"
cp "${WATCHDOG_BIN}" "${STAGE_DIR}/watchdog"
cp "${SHARD_BIN}" "${STAGE_DIR}/shard"
cp "${PROXY_BIN}" "${STAGE_DIR}/proxy"
cp "${PREBUILT_WEB_NODE_PATH}" "${STAGE_DIR}/Web.node"
rm -rf "${STAGE_DIR}/cartograph"
cp -a "${CARTOGRAPH_DIR}" "${STAGE_DIR}/cartograph"

echo "Staged prebuilt artifacts in ${STAGE_DIR}"
echo "  watchdog: ${WATCHDOG_BIN}"
echo "  shard:    ${SHARD_BIN}"
echo "  proxy:    ${PROXY_BIN}"
echo "  Web.node: ${PREBUILT_WEB_NODE_PATH}"
