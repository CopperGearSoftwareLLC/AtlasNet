
NAME_OF_THIS_FILE="AtlasNetVars.sh"

BUILD_CONFIG="DebugDocker"
GOD_IMAGE_NAME="god-image"
GOD_DOCKER_FILE="DockerFile.god"
GOD_CONTAINER_NAME="god"

PARTITION_IMAGE_NAME="partition-image"
PARTITION_DOCKER_FILE="DockerFile.partition"
PARTITION_CONTAINER_NAME="partition"
IN_COMPOSE_PARTITION_PORT="80085"

GAME_SERVER_IMAGE="game-server-image"
GAME_SERVER_DOCKER_FILE="DockerFile.gameserver"
GAME_SERVER_CONTAINER_NAME="game-server"

GDBSERVER_INTERNAL_PORT="1234"

GAMESERVER_EXECUTABLE_PATH="UNDEFINED"
GAMESERVER_EXECUTABLE_NAME="UNDEFINED"
DOCKER_FILES_PATH="docker"

BASE_DOCKER_FILE="
# Use an official Ubuntu base image
FROM ubuntu:25.04
# Set working directory
WORKDIR /app

# Copy local files into the image
# Install dependencies
RUN apt-get update && \
apt-get install -y gdbserver docker.io libprotobuf-dev protobuf-compiler libssl-dev libcurl4-openssl-dev curl tini"


GOD_DOCKER_FILE_CONTENT="$BASE_DOCKER_FILE
COPY ../bin/${BUILD_CONFIG}/God/God /app/
COPY $NAME_OF_THIS_FILE /app/
COPY "Start.sh" /app/

# For GDBserver
EXPOSE 1234 
# Set default command
CMD [\"gdbserver\", \"0.0.0.0:1234\", \"god\"]
"
PARTITION_DOCKER_FILE_CONTENT="$BASE_DOCKER_FILE

COPY ../bin/${BUILD_CONFIG}/Partition/Partition /app/
COPY {GAMESERVER_EXECUTABLE_PATH} /app/
COPY $NAME_OF_THIS_FILE /app/
COPY "Start.sh" /app/

CMD /bin/bash -c \"gdbserver :1235 Partition & gdbserver :1236 {GAMESERVER_EXECUTABLE_NAME}\"
"
