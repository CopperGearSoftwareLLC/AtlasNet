#!/usr/bin/env bash
set -euo pipefail

dump_vcpkg_failure_logs() {
  local status="$1"
  local vcpkg_root="${VCPKG_ROOT-/opt/vcpkg/2026.01.16}"
  local buildtrees_dir="$vcpkg_root/buildtrees"

  echo "AtlasNet ARM64 CodeBuild failed with status $status"

  if [[ -d "$buildtrees_dir/openssl" ]]; then
    while IFS= read -r log_file; do
      echo "==> Tail of $log_file"
      tail -n 120 "$log_file" || true
      echo "==> End tail of $log_file"
    done < <(find "$buildtrees_dir/openssl" -maxdepth 1 -type f -name '*.log' | sort)
  fi
}

trap 'status=$?; if [[ $status -ne 0 ]]; then dump_vcpkg_failure_logs "$status"; fi' EXIT

echo "Starting AtlasNet ARM64 CodeBuild on $(date -u)"
echo "Source version ${CODEBUILD_RESOLVED_SOURCE_VERSION-unknown}"
echo "Detected architecture $(uname -m)"

if [[ -f /etc/os-release ]]; then
  # shellcheck disable=SC1091
  . /etc/os-release
  echo "Detected OS ${PRETTY_NAME-unknown}"
fi

if [[ -z "${HOME-}" || ! -d "${HOME-}" || ! -r "${HOME-}" || ! -w "${HOME-}" ]]; then
  export HOME=/tmp/codebuild-home
fi

mkdir -p "$HOME"
export XDG_CACHE_HOME="${XDG_CACHE_HOME-$HOME/.cache}"
export VCPKG_DOWNLOADS="${VCPKG_DOWNLOADS-$XDG_CACHE_HOME/vcpkg/downloads}"
export VCPKG_DISABLE_METRICS=1
mkdir -p "$XDG_CACHE_HOME" "$VCPKG_DOWNLOADS"

echo "Using HOME $HOME"
for header_path in \
  /usr/include/linux/version.h \
  /usr/include/linux/limits.h \
  /usr/include/asm/errno.h \
  /usr/include/openssl/opensslconf.h; do
  if [[ -e "$header_path" ]]; then
    echo "Found header $header_path"
  else
    echo "Missing header $header_path"
  fi
done

if [[ "$(uname -m)" != "aarch64" ]]; then
  echo "ERROR: This project must run on an ARM64 CodeBuild environment."
  exit 1
fi

if [[ -z "${DOCKERHUB_NAMESPACE-}" ]]; then
  echo "ERROR: Set DOCKERHUB_NAMESPACE in the CodeBuild project."
  exit 1
fi

if [[ -z "${DOCKERHUB_USERNAME-}" ]]; then
  echo "ERROR: Set DOCKERHUB_USERNAME in the CodeBuild project."
  exit 1
fi

if [[ -z "${DOCKERHUB_TOKEN-}" ]]; then
  echo "ERROR: Set DOCKERHUB_TOKEN in the CodeBuild project via Secrets Manager."
  exit 1
fi

docker version
docker info

CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE-Release}" \
ATLASNET_IMAGE_TAG="${ATLASNET_IMAGE_TAG-latest}" \
ATLASNET_IMAGE_TAG_AMD64="${ATLASNET_IMAGE_TAG_AMD64-latest-amd64}" \
ATLASNET_IMAGE_TAG_ARM64="${ATLASNET_IMAGE_TAG_ARM64-latest-arm64}" \
JOBS="${JOBS-4}" \
make -C k8s/pi-native ci

echo "AtlasNet ARM64 CodeBuild finished on $(date -u)"
