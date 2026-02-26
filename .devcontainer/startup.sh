#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-}"

case "$MODE" in
  # TigerVNC's headless X server WITH VNC built-in (supports remote resize requests)
  tigervnc)
    export DISPLAY="${DISPLAY:-:1}"

    # You can override these at container run-time:
    #   -e VNC_GEOMETRY=2560x1440  -e VNC_DEPTH=24  -e VNC_PORT=5901
    GEOM="${VNC_GEOMETRY:-${VNC_RESOLUTION:-1920x1080}}"
    DEPTH="${VNC_DEPTH:-24}"
    PORT="${VNC_PORT:-5901}"
    NAME="${VNC_DESKTOP_NAME:-devcontainer}"

    # Clean stale locks if the container restarted
    rm -f "/tmp/.X${DISPLAY#:}-lock" "/tmp/.X11-unix/X${DISPLAY#:}" 2>/dev/null || true

    # Notes:
    # -SecurityTypes None => matches your old "-nopw" behavior (DEV ONLY)
    # -AcceptSetDesktopSize => allows client-driven resolution changes (remote resize)
    exec Xtigervnc "$DISPLAY" \
      -geometry "$GEOM" \
      -depth "$DEPTH" \
      -rfbport "$PORT" \
      -SecurityTypes None \
      -AcceptSetDesktopSize \
      -AlwaysShared \
      -desktop "$NAME"
    ;;

  fluxbox)
    export DISPLAY="${DISPLAY:-:1}"
    for i in $(seq 1 80); do xdpyinfo -display "$DISPLAY" >/dev/null 2>&1 && break || sleep 0.1; done
    exec fluxbox
    ;;

  novnc)
    # noVNC web client on :6080 -> VNC :5901
    #
    # In noVNC UI, set Scaling mode to "Remote Resizing" to have it request
    # desktop size changes as your browser window changes.
    # (noVNC calls this "resizeSession", disabled by default.)
    exec websockify --web=/usr/share/novnc/ 6080 127.0.0.1:5901
    ;;

  hostproxy)
    LISTEN_PORT="${2:?usage: $0 hostproxy <listen-port> [target-host] [target-port]}"
    TARGET_HOST="${3:-host.docker.internal}"
    TARGET_PORT="${4:-$LISTEN_PORT}"
    exec socat "TCP-LISTEN:${LISTEN_PORT},fork,reuseaddr" "TCP:${TARGET_HOST}:${TARGET_PORT}"
    ;;

  *)
    echo "usage: $0 {tigervnc|fluxbox|novnc|hostproxy}"
    exit 2
    ;;
esac
