
NAME_OF_THIS_FILE="KDNetVars.sh"

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

PARTITION_GAMESERVER_COMPOSE_FILE="partition-gameserver-docker-compose.yml"

GDBSERVER_INTERNAL_PORT="1234"

EXECUTABLE_PATH="UNDEFINED"
DOCKER_FILES_PATH="docker"

BASE_DOCKER_FILE="
# Use an official Ubuntu base image
FROM ubuntu:25.04
# Set working directory
WORKDIR /app

# Copy local files into the image
# Install dependencies
RUN apt-get update && \
apt-get install -y gdbserver docker.io libprotobuf-dev protobuf-compiler libssl-dev libcurl4-openssl-dev curl tini


COPY {EXECUTABLE_PATH} /app/
COPY $NAME_OF_THIS_FILE /app/
COPY "Start.sh" /app/

# For GDBserver
EXPOSE 1234 
# Set default command use debugger or not (warning: debugger wont let sigterm go to program)
CMD [\"gdbserver\", \"0.0.0.0:1234\", \"/app/{EXECUTABLE_NAME}\"]
# CMD [\"/app/{EXECUTABLE_NAME}\"]

"

BASE_PARTITION_COMPOSE_DOCKER_FILE="
version: \"3.9\"
services:
  ${GAME_SERVER_CONTAINER_NAME}:
    image: ${GAME_SERVER_IMAGE}
    container_name: ${GAME_SERVER_CONTAINER_NAME}
    restart: \"no\"
    networks:
      - shared_network
    ports:
      - \":${GDBSERVER_INTERNAL_PORT:-1234}\"   # host:container

  partition:
    image: ${PARTITION_IMAGE_NAME}:latest
    container_name: partition_container
    restart: \"no\"
    networks:
      - shared_network
    ports:
      - \":${GDBSERVER_INTERNAL_PORT:-1234}\"   # host:container

networks:
  shared_network:
    driver: bridge
"