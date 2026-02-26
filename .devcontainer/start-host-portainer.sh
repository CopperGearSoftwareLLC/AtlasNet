#!/usr/bin/env bash
set -euo pipefail

PORTAINER_NAME="${PORTAINER_NAME:-portainer}"
PORTAINER_HTTP_PORT="${PORTAINER_HTTP_PORT:-9000}"
PORTAINER_HTTPS_PORT="${PORTAINER_HTTPS_PORT:-9443}"
PORTAINER_DATA_VOLUME="${PORTAINER_DATA_VOLUME:-portainer_data}"

if ! command -v docker >/dev/null 2>&1; then
    echo "[portainer] docker CLI not found; skipping."
    exit 0
fi

DOCKER=(docker)
if ! docker info >/dev/null 2>&1; then
    if command -v sudo >/dev/null 2>&1 && sudo -n docker info >/dev/null 2>&1; then
        DOCKER=(sudo docker)
    else
        echo "[portainer] docker daemon not reachable yet; skipping."
        exit 0
    fi
fi

if ! "${DOCKER[@]}" info >/dev/null 2>&1; then
    echo "[portainer] docker daemon not reachable yet; skipping."
    exit 0
fi

if "${DOCKER[@]}" ps --format '{{.Names}}' | grep -Fxq "${PORTAINER_NAME}"; then
    echo "[portainer] ${PORTAINER_NAME} is already running."
    exit 0
fi

if "${DOCKER[@]}" ps -a --format '{{.Names}}' | grep -Fxq "${PORTAINER_NAME}"; then
    echo "[portainer] removing stale container ${PORTAINER_NAME}..."
    "${DOCKER[@]}" rm -f "${PORTAINER_NAME}" >/dev/null || true
fi

"${DOCKER[@]}" volume inspect "${PORTAINER_DATA_VOLUME}" >/dev/null 2>&1 || \
    "${DOCKER[@]}" volume create "${PORTAINER_DATA_VOLUME}" >/dev/null

echo "[portainer] starting ${PORTAINER_NAME} on host ports ${PORTAINER_HTTP_PORT}/${PORTAINER_HTTPS_PORT}..."
if ! "${DOCKER[@]}" run -d \
    --name "${PORTAINER_NAME}" \
    --restart unless-stopped \
    -p "${PORTAINER_HTTP_PORT}:9000" \
    -p "${PORTAINER_HTTPS_PORT}:9443" \
    -v /var/run/docker.sock:/var/run/docker.sock \
    -v "${PORTAINER_DATA_VOLUME}:/data" \
    portainer/portainer-ce:latest >/dev/null; then
    echo "[portainer] failed to start (ports may already be in use)."
fi
