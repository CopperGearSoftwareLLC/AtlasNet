#!/usr/bin/env bash
set -euo pipefail

echo "Starting AtlasNet ARM64 CodeBuild on $(date -u)"
echo "Source version ${CODEBUILD_RESOLVED_SOURCE_VERSION-unknown}"
echo "Detected architecture $(uname -m)"

if [[ -f /etc/os-release ]]; then
  # shellcheck disable=SC1091
  . /etc/os-release
  echo "Detected OS ${PRETTY_NAME-unknown}"
fi

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
