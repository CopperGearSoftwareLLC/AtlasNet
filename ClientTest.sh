#!/bin/bash
set -e

# ==========================================
# CONFIG
# ==========================================
IMAGE_NAME="samplegame-client"
IMAGE_TAG="latest"
DOCKERFILE_PATH="Tests/SampleGame/Client.DockerFile"
SERVICE_NAME="samplegame_client"
REPLICAS=5

# ==========================================
# Build Image
# ==========================================
echo "🐳 Building Docker image: $IMAGE_NAME:$IMAGE_TAG"
docker build -t "${IMAGE_NAME}:${IMAGE_TAG}" -f "$DOCKERFILE_PATH" .

# ==========================================
# Deploy Service (host network)
# ==========================================
echo "🚀 Deploying service with ${REPLICAS} replicas using HOST networking..."

SOCK_MOUNT="type=bind,src=/var/run/docker.sock,dst=/var/run/docker.sock"

if docker service ls | grep -q "$SERVICE_NAME"; then
    echo "🔄 Updating existing service..."
    docker service update \
        --image "${IMAGE_NAME}:${IMAGE_TAG}" \
        --replicas "$REPLICAS" \
        --network-add host \
        --mount-add "$SOCK_MOUNT" \
        "$SERVICE_NAME"
else
    echo "🆕 Creating new service..."
    docker service create \
        --name "$SERVICE_NAME" \
        --replicas "$REPLICAS" \
        --network host \
        --mount "$SOCK_MOUNT" \
        "${IMAGE_NAME}:${IMAGE_TAG}"
fi

echo "✅ Done!"
docker service ps "$SERVICE_NAME"
