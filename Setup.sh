#!/bin/bash
set -e  # Exit immediately if any command fails
. ./KDNetVars.sh   

# Check if at least one argument is provided
if [ $# -lt 1 ]; then
    echo "Usage: $0 <GameServerExeName>"
    exit 1
fi

build_image_function()
{
    local CONTENT=$1
    mkdir -p "$DOCKER_FILES_PATH"
    echo "$CONTENT" > "$DOCKER_FILES_PATH/$DOCKER_FILE"
    echo "$IMAGE_NAME Dockerfile created: $DOCKER_FILES_PATH/$DOCKER_FILE"

    echo "Building Docker image $IMAGE_NAME:latest"
    if ! docker build -f "$DOCKER_FILES_PATH/$DOCKER_FILE" -t "$IMAGE_NAME:latest" .; then
        echo "Failed to build Docker image $IMAGE_NAME"
        exit 1
    fi
}

build_compose_function()
{
    echo "$BASE_PARTITION_COMPOSE_DOCKER_FILE" > "$DOCKER_FILES_PATH/$PARTITION_GAMESERVER_COMPOSE_FILE"
}

echo "Compiling"
if ! ./premake5 gmake; then
    echo "premake5 failed"
    exit 1
fi

if ! make -C ./build/ config=${BUILD_CONFIG,,} -j16; then
    echo "Make failed"
    exit 1
fi

echo "Building God Image"
DOCKER_FILE=$GOD_DOCKER_FILE
IMAGE_NAME=$GOD_IMAGE_NAME
echo "$GOD_DOCKER_FILE_CONTENT"
build_image_function "$GOD_DOCKER_FILE_CONTENT"

echo "Building Partition Image"
DOCKER_FILE=$PARTITION_DOCKER_FILE
IMAGE_NAME=$PARTITION_IMAGE_NAME
GAMESERVER_EXECUTABLE_NAME=$1
GAMESERVER_EXECUTABLE_PATH="../bin/${BUILD_CONFIG}/$GAMESERVER_EXECUTABLE_NAME/$GAMESERVER_EXECUTABLE_NAME"

PARTITION_DOCKER_FILE_CONTENT="${PARTITION_DOCKER_FILE_CONTENT//\{GAMESERVER_EXECUTABLE_PATH\}/$GAMESERVER_EXECUTABLE_PATH}"
PARTITION_DOCKER_FILE_CONTENT="${PARTITION_DOCKER_FILE_CONTENT//\{GAMESERVER_EXECUTABLE_NAME\}/$GAMESERVER_EXECUTABLE_NAME}"

build_image_function "$PARTITION_DOCKER_FILE_CONTENT"

sleep 2
