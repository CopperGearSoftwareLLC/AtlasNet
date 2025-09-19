#!/bin/bash
GOD_NAME="god"

echo "[Watcher] Script started at $(date)"
echo "[Watcher] Waiting for container '$GOD_NAME' to stop..."

# Listen for the first "die" event only
docker events \
  --filter "container=$GOD_NAME" \
  --filter "event=die" \
  --format '{{.Actor.Attributes.name}}' \
  | while read cname; do
      if [ "$cname" = "$GOD_NAME" ]; then
          echo "[Watcher] God container stopped → cleaning up partitions..."
          docker rm -f $(docker ps -aq --filter "name=partition_") 2>/dev/null || true
          echo "[Watcher] Cleanup complete → exiting."
          break
      fi
    done
