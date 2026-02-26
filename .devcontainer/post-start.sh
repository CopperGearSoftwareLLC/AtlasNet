#!/usr/bin/env bash
set -euo pipefail

# Ensure the vscode user can talk to /var/run/docker.sock.
/bin/bash .devcontainer/fix-docker-sock-group.sh || true

# Keep parity with previous DinD workflow: auto-start Portainer.
/bin/bash .devcontainer/start-host-portainer.sh || true
