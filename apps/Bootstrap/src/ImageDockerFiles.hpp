#pragma once

#include "MiscDockerFiles.hpp"
#include "misc/String_utils.hpp"

DOCKER_FILE_DEF ATLASNET_STACK_RAW = R"(
version: "3.8"

services:
  ${GOD_SERVICE_NAME}:
    image: ${REGISTRY_ADDR}${GOD_IMAGE_NAME}
    networks: [${ATLASNET_NETWORK_NAME}]
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock
    deploy:
      mode: replicated
      replicas: ${GOD_REPLICA_NUM}
      restart_policy:
        condition: on-failure

  ${PARTITION_SERVICE_NAME}:
    image: ${REGISTRY_ADDR}${PARTITION_IMAGE_NAME}:latest
    networks: [${ATLASNET_NETWORK_NAME}]
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock
    deploy:
      mode: replicated
      replicas: 0   # 0 replicas as requested (exists, but wont run)
      restart_policy:
        condition: on-failure

  ${CLUSTERDB_SERVICE_NAME}:
    image: ${REGISTRY_ADDR}${CLUSTERDB_IMAGE_NAME}:latest
    networks: [${ATLASNET_NETWORK_NAME}]
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock
    deploy:
      mode: replicated
      replicas: 1
      restart_policy:
        condition: on-failure
  ${REDIS_SERVICE_NAME}:
    image: redis:7
    command: ["redis-server", "--appendonly", "yes"]
    networks: [${ATLASNET_NETWORK_NAME}]
    deploy:
      mode: replicated
      replicas: 1
      restart_policy:
        condition: on-failure

  ${GAME_COORDINATOR_SERVICE_NAME}:
    image: ${REGISTRY_ADDR}${GAME_COORDINATOR_IMAGE_NAME}:latest
    networks: [${ATLASNET_NETWORK_NAME}]
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock
    ports:
      - target: ${GAME_COORDINATOR_PORT_TARGET}      # container port
        published: ${GAME_COORDINATOR_PORT_PUBLISHED}   # host port
        protocol: udp
        mode: ingress
    deploy:
      mode: replicated
      replicas: 1
      restart_policy:
        condition: on-failure

  ${DEMIGOD_SERVICE_NAME}:
    image: ${REGISTRY_ADDR}${DEMIGOD_IMAGE_NAME}:latest
    networks: [${ATLASNET_NETWORK_NAME}]
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock
    ports:
      - target: 2555
        published: 2555
        protocol: tcp
        mode: ingress
    deploy:
      mode: replicated
      replicas: 1
      restart_policy:
        condition: on-failure

networks:
  ${ATLASNET_NETWORK_NAME}:
    external: true
    name: ${ATLASNET_NETWORK_NAME}
)";
DOCKER_FILE_DEF ATLASNET_STACK =
	MacroParse(ATLASNET_STACK_RAW,
			   {{"GOD_IMAGE_NAME", _GOD_IMAGE_NAME},
				{"GOD_SERVICE_NAME", _GOD_SERVICE_NAME},
				{"ATLASNET_NETWORK_NAME", _ATLASNET_NETWORK_NAME},
				{"GOD_REPLICA_NUM", std::to_string(1)},
				{"REGISTRY_ADDR", ""},	//"localhost/"
				{"PARTITION_IMAGE_NAME", _PARTITION_IMAGE_NAME},
				{"PARTITION_SERVICE_NAME", _PARTITION_SERVICE_NAME},
				{"CLUSTERDB_IMAGE_NAME", _CLUSTERDB_IMAGE_NAME},
				{"CLUSTERDB_SERVICE_NAME", _CLUSTERDB_SERVICE_NAME},
				{"GAME_COORDINATOR_IMAGE_NAME", _GAME_COORDINATOR_IMAGE_NAME},
				{"GAME_COORDINATOR_SERVICE_NAME", _GAME_COORDINATOR_SERVICE_NAME},
				{"GAME_COORDINATOR_PORT_TARGET", std::to_string(_GAME_COORDINATOR_PORT)},
				{"GAME_COORDINATOR_PORT_PUBLISHED", std::to_string(_GAME_COORDINATOR_PORT)},
				{"DEMIGOD_IMAGE_NAME", _DEMIGOD_IMAGE_NAME},
				{"DEMIGOD_SERVICE_NAME", _DEMIGOD_SERVICE_NAME},
				{"REDIS_SERVICE_NAME", _REDIS_SERVICE_NAME}});
DOCKER_FILE_DEF Generic_Builder_Header = R"(
FROM ${OS_VERSION} AS builder
WORKDIR ${WORKDIR}

)";

DOCKER_FILE_DEF Generic_Run_Header = R"(
FROM ${OS_VERSION} AS runner
WORKDIR ${WORKDIR}
)";

DOCKER_FILE_DEF COPY_ATLASNET_SRC = R"(
COPY CMakeLists.txt CMakeLists.txt
COPY apps ./apps
COPY libs ./libs
)";
DOCKER_FILE_DEF BUILD_ATLASNET_SRC = R"(
RUN mkdir build
RUN  --mount=type=cache,target=/build \ 
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
RUN  --mount=type=cache,target=/build/apps \
    --mount=type=cache,target=/build/libs \
     cmake --build build --parallel --target ${BUILD_PROJECT} 
RUN --mount=type=cache,target=/build \
    cmake --install build --component ${BUILD_PROJECT} --prefix ${WORKDIR}/
)";

DOCKER_FILE_DEF GodDockerFile = MacroParse(
	Generic_Builder_Header + GET_REQUIRED_BUILD_PKGS + VCPKG_Install + COPY_ATLASNET_SRC +
		BUILD_ATLASNET_SRC + Generic_Run_Header + GET_REQUIRED_RUN_PKGS + CopyBuild_StripLib +
		R"(ENTRYPOINT ["${WORKDIR}/God"])",
	{{"OS_VERSION", _DOCKER_OS_}, {"WORKDIR", _DOCKER_WORKDIR_}, {"BUILD_PROJECT", "God"}});

DOCKER_FILE_DEF DemiGodDockerFile = MacroParse(
	Generic_Builder_Header + GET_REQUIRED_BUILD_PKGS + VCPKG_Install + COPY_ATLASNET_SRC +
		BUILD_ATLASNET_SRC + Generic_Run_Header + GET_REQUIRED_RUN_PKGS + CopyBuild_StripLib +
		R"(ENTRYPOINT ["${WORKDIR}/DemiGod"])",
	{{"OS_VERSION", _DOCKER_OS_}, {"WORKDIR", _DOCKER_WORKDIR_}, {"BUILD_PROJECT", "DemiGod"}});

DOCKER_FILE_DEF ClusterDBDockerFile = MacroParse(
	Generic_Builder_Header + GET_REQUIRED_BUILD_PKGS + VCPKG_Install + COPY_ATLASNET_SRC +
		BUILD_ATLASNET_SRC + Generic_Run_Header + GET_REQUIRED_RUN_PKGS + CopyBuild_StripLib +
		R"(ENTRYPOINT ["${WORKDIR}/ClusterDB"])",
	{{"OS_VERSION", _DOCKER_OS_}, {"WORKDIR", _DOCKER_WORKDIR_}, {"BUILD_PROJECT", "ClusterDB"}});

DOCKER_FILE_DEF GameCoordinatorDockerFile = MacroParse(
	Generic_Builder_Header + GET_REQUIRED_BUILD_PKGS + VCPKG_Install + COPY_ATLASNET_SRC +
		BUILD_ATLASNET_SRC + Generic_Run_Header + GET_REQUIRED_RUN_PKGS + CopyBuild_StripLib +
		R"(ENTRYPOINT [""])",
	{{"OS_VERSION", _DOCKER_OS_}, {"WORKDIR", _DOCKER_WORKDIR_}, {"BUILD_PROJECT", "God"}});

DOCKER_FILE_DEF PartitionDockerFile = MacroParse(
	Generic_Builder_Header + GET_REQUIRED_BUILD_PKGS + VCPKG_Install + COPY_ATLASNET_SRC +
		BUILD_ATLASNET_SRC + Generic_Run_Header + GET_REQUIRED_RUN_PKGS + CopyBuild_StripLib,
	{{"OS_VERSION", _DOCKER_OS_}, {"WORKDIR", _DOCKER_WORKDIR_}, {"BUILD_PROJECT", "Partition"}});