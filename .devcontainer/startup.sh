#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-}"

case "$MODE" in
  dockerd)
    mkdir -p /var/run /var/lib/docker

    dockerd \
      --host=unix:///var/run/docker.sock \
      --storage-driver=overlay2 \
      --iptables=true \
      --log-level=error \
      &

    for i in $(seq 1 120); do
      if docker info >/dev/null 2>&1; then break; fi
      sleep 0.25
    done

    docker volume inspect portainer_data >/dev/null 2>&1 || docker volume create portainer_data >/dev/null

    if ! docker ps --format '{{.Names}}' | grep -qx 'portainer'; then
      docker rm -f portainer >/dev/null 2>&1 || true
      docker run -d \
        --name portainer \
        --restart=unless-stopped \
        -p 9000:9000 \
        -p 9443:9443 \
        -v /var/run/docker.sock:/var/run/docker.sock \
        -v portainer_data:/data \
        portainer/portainer-ce:latest
    fi

    wait -n
    ;;

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
    # (noVNC calls this "resizeSession", disabled by default.) :contentReference[oaicite:1]{index=1}
    exec websockify --web=/usr/share/novnc/ 6080 127.0.0.1:5901
    ;;

  *)
    echo "usage: $0 {dockerd|tigervnc|fluxbox|novnc}"
    exit 2
    ;;
esac
