#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-}"

case "$MODE" in
    dockerd)
    mkdir -p /var/run /var/lib/docker

    # start dockerd in background so we can provision portainer
    dockerd \
      --host=unix:///var/run/docker.sock \
      --storage-driver=overlay2 \
      --iptables=true \
      --log-level=error \
      &

    # wait for docker to be ready
    for i in $(seq 1 120); do
      if docker info >/dev/null 2>&1; then break; fi
      sleep 0.25
    done

    # create a persistent named volume for Portainer data
    docker volume inspect portainer_data >/dev/null 2>&1 || docker volume create portainer_data >/dev/null

    # run (or recreate if missing) Portainer attached to this DinD daemon
    if ! docker ps --format '{{.Names}}' | grep -qx 'portainer'; then
      # clean up any stopped container with same name
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

    # keep the foreground process as dockerd
    wait -n
    ;;


  xvfb)
    export DISPLAY="${DISPLAY:-:1}"
    exec Xvfb "$DISPLAY" -screen 0 1920x1080x24 -ac +extension GLX +render -noreset
    ;;

  fluxbox)
    export DISPLAY="${DISPLAY:-:1}"
    # wait for Xvfb
    for i in $(seq 1 50); do xdpyinfo -display "$DISPLAY" >/dev/null 2>&1 && break || sleep 0.1; done
    exec fluxbox
    ;;

  x11vnc)
    export DISPLAY="${DISPLAY:-:1}"
    for i in $(seq 1 50); do xdpyinfo -display "$DISPLAY" >/dev/null 2>&1 && break || sleep 0.1; done
    # No password (local dev). Add -passwdfile if you want.
    exec x11vnc -display "$DISPLAY" -forever -shared -nopw -rfbport 5901 -xkb
    ;;

  novnc)
    # noVNC web client on :6080 -> VNC :5901
    exec websockify --web=/usr/share/novnc/ 6080 127.0.0.1:5901
    ;;

  *)
    echo "usage: $0 {dockerd|xvfb|fluxbox|x11vnc|novnc}"
    exit 2
    ;;
esac
