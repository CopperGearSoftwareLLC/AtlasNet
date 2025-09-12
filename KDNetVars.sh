
BUILD_CONFIG="DebugDocker"

GOD_IMAGE_NAME="god-image"
GOD_DOCKER_FILE="DockerFile.god"
GOD_CONTAINER_BASE_NAME="god"

PARTITION_IMAGE_NAME="partition-image"
PARTITION_DOCKER_FILE="DockerFile.partition"
PARTITION_CONTAINER_NAME="partition"

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
apt-get install -y gdbserver docker.io libprotobuf-dev protobuf-compiler libssl-dev


COPY {EXECUTABLE_PATH} /app/
# For GDBserver
EXPOSE 1234 
# Set default command
CMD [\"gdbserver\", \":${GDBSERVER_INTERNAL_PORT}\",\"{EXECUTABLE_NAME}\"]
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