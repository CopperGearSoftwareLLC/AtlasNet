#!/usr/bin/env bash
set -euo pipefail

SOCKET_PATH="${1:-/var/run/docker.sock}"
TARGET_USER="${2:-${USER:-vscode}}"

if [[ ! -S "${SOCKET_PATH}" ]]; then
    exit 0
fi

run_sudo_non_interactive() {
    if command -v sudo >/dev/null 2>&1 && sudo -n true >/dev/null 2>&1; then
        sudo -n "$@"
        return $?
    fi
    echo "[docker-sock] sudo -n unavailable; skipping: $*"
    return 1
}

SOCKET_GID="$(stat -c '%g' "${SOCKET_PATH}")"
GROUP_NAME="$(getent group "${SOCKET_GID}" | cut -d: -f1 || true)"

if [[ -z "${GROUP_NAME}" ]]; then
    GROUP_NAME="docker-host"
    run_sudo_non_interactive groupadd --gid "${SOCKET_GID}" "${GROUP_NAME}" 2>/dev/null || true
fi

if ! id -nG "${TARGET_USER}" | tr ' ' '\n' | grep -Fxq "${GROUP_NAME}"; then
    if run_sudo_non_interactive usermod -aG "${GROUP_NAME}" "${TARGET_USER}"; then
        echo "Added ${TARGET_USER} to ${GROUP_NAME} (gid ${SOCKET_GID})."
    else
        echo "[docker-sock] Could not add ${TARGET_USER} to ${GROUP_NAME} automatically."
    fi
fi

# Grant immediate access for already-running processes (first attach) when possible.
if command -v setfacl >/dev/null 2>&1; then
    if run_sudo_non_interactive setfacl -m "u:${TARGET_USER}:rw" "${SOCKET_PATH}"; then
        echo "[docker-sock] Granted immediate ACL access to ${TARGET_USER} on ${SOCKET_PATH}."
        exit 0
    fi
fi

if [[ -w "${SOCKET_PATH}" ]]; then
    exit 0
fi

echo "[docker-sock] ${TARGET_USER} may need to reconnect for docker group changes to apply."
