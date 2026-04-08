#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
source_env

need_cmd docker

name="$(registry_container_name)"
if docker ps --format '{{.Names}}' | grep -Fx "$name" >/dev/null 2>&1; then
  docker stop "$name" >/dev/null
  echo "Stopped registry container: $name"
else
  echo "Registry container is not running: $name"
fi
