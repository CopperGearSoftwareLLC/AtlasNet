#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HELPER_IMAGE="${ATLASNET_LATENCY_HELPER_IMAGE:-atlasnet-latency-helper:latest}"
HELPER_NAME="${ATLASNET_LATENCY_HELPER_NAME:-atlasnet_latency_helper}"
DOCKERFILE_PATH="${SCRIPT_DIR}/LatencyHelper.dockerfile"
VERBOSE="${ATLASNET_LATENCY_VERBOSE:-0}"

die() {
  echo "ERROR: $*" >&2
  exit 1
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "Missing required command: $1"
}

need_cmd docker
docker info >/dev/null 2>&1 || die "Docker daemon is not reachable."
[[ -f "$DOCKERFILE_PATH" ]] || die "Missing helper Dockerfile: $DOCKERFILE_PATH"

if [[ "$VERBOSE" == "1" ]]; then
  echo "==> Building latency helper image '${HELPER_IMAGE}'..."
fi
docker build -t "$HELPER_IMAGE" -f "$DOCKERFILE_PATH" "$SCRIPT_DIR" >/dev/null

if docker ps -a --format '{{.Names}}' | grep -Fxq "$HELPER_NAME"; then
  if [[ "$VERBOSE" == "1" ]]; then
    echo "==> Replacing existing latency helper container '${HELPER_NAME}'..."
  fi
  docker rm -f "$HELPER_NAME" >/dev/null 2>&1 || true
fi

if [[ "$VERBOSE" == "1" ]]; then
  echo "==> Starting latency helper container '${HELPER_NAME}'..."
fi
docker run -d \
  --name "$HELPER_NAME" \
  --restart unless-stopped \
  --privileged \
  --pid=host \
  -v /var/run/docker.sock:/var/run/docker.sock \
  "$HELPER_IMAGE" >/dev/null

if [[ "$VERBOSE" == "1" ]]; then
  echo "==> Latency helper ready: ${HELPER_NAME}"
fi
