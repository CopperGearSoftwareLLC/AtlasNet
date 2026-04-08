#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
source_env

need_cmd docker
docker info >/dev/null 2>&1 || die "Docker daemon is not reachable."

name="$(registry_container_name)"
data_dir="$(registry_data_dir)"
port="$(registry_port)"

mkdir -p "$data_dir"

if docker ps --format '{{.Names}}' | grep -Fx "$name" >/dev/null 2>&1; then
  echo "Registry is already running: $name"
  exit 0
fi

if docker ps -a --format '{{.Names}}' | grep -Fx "$name" >/dev/null 2>&1; then
  echo "Starting existing registry container: $name"
  docker start "$name" >/dev/null
else
  echo "Starting local registry on 0.0.0.0:${port}"
  docker run -d \
    --restart unless-stopped \
    --name "$name" \
    -p "${port}:5000" \
    -v "$data_dir:/var/lib/registry" \
    registry:3 >/dev/null
fi

echo "Registry ready:"
echo " - local push endpoint: $(registry_local_hostport)"
echo " - node pull endpoint:  $(registry_advertise_hostport)"
