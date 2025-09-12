#!/bin/bash
#!/bin/bash
. ./KDNetVars.sh   
# Run docker-compose


# Path to Partition



# Check if at least one argument is provided
if [ $# -lt 1 ]; then
    echo "Usage: $0 <GameServerExeName>"
    exit 1
fi

 build_image_function()
{

    # Format Partition's Docker file
    CONTENT="${BASE_DOCKER_FILE//\{EXECUTABLE_PATH\}/$EXECUTABLE_PATH}"
    CONTENT="${CONTENT//\{EXECUTABLE_NAME\}/$EXECUTABLE_NAME}"

    # --- Write to Dockerfile ---
    mkdir -p "$DOCKER_FILES_PATH"
    echo "$CONTENT" > "$DOCKER_FILES_PATH"/"$DOCKER_FILE"

    echo "$IMAGE_NAME Dockerfile created: $DOCKER_FILES_PATH/$DOCKER_FILE"

    # --- Build Docker Image ---
    docker build -f $DOCKER_FILES_PATH/$DOCKER_FILE -t $IMAGE_NAME:latest .
}

build_compose_function()
{
    echo "$BASE_PARTITION_COMPOSE_DOCKER_FILE" > "$DOCKER_FILES_PATH"/"$PARTITION_GAMESERVER_COMPOSE_FILE"
}



echo "Building God Image"
EXECUTABLE_PATH="../bin/${BUILD_CONFIG}/God/God"
EXECUTABLE_NAME="God"
DOCKER_FILE=$GOD_DOCKER_FILE
IMAGE_NAME=$GOD_IMAGE_NAME

 build_image_function

echo "Building Partition Image"
EXECUTABLE_PATH="../bin/${BUILD_CONFIG}/Partition/Partition"
EXECUTABLE_NAME="Partition"
DOCKER_FILE=$PARTITION_DOCKER_FILE
IMAGE_NAME=$PARTITION_IMAGE_NAME

 build_image_function

EXE_NAME="$1"
echo "Building GameServer Image with $EXE_NAME"
EXECUTABLE_PATH="../bin/${BUILD_CONFIG}/$EXE_NAME/$EXE_NAME"
EXECUTABLE_NAME="$EXE_NAME"
DOCKER_FILE=$GAME_SERVER_DOCKER_FILE
IMAGE_NAME=$GAME_SERVER_IMAGE

 build_image_function



 build_compose_function


