#pragma once

#include "MiscDockerFiles.hpp"
#include "misc/String_utils.hpp"

DOCKER_FILE_DEF ATLASNET_STACK_RAW = R"(
version: "3.8"

services:
  ${GOD_SERVICE_NAME}:
    image: ${REGISTRY_ADDR_OPT}${GOD_IMAGE_NAME}
    networks: [${ATLASNET_NETWORK_NAME}]
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock
    deploy:
      mode: replicated
      replicas: ${GOD_REPLICA_NUM}
      restart_policy:
        condition: on-failure

  ${PARTITION_SERVICE_NAME}:
    image: ${REGISTRY_ADDR_OPT}${GAME_SERVER_IMAGE_NAME}:latest
    networks: [${ATLASNET_NETWORK_NAME}]
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock
    deploy:
      mode: replicated
      replicas: 0   # 0 replicas as requested (exists, but wont run)
      restart_policy:
        condition: on-failure

  ${CLUSTERDB_SERVICE_NAME}:
    image: ${REGISTRY_ADDR_OPT}${CLUSTERDB_IMAGE_NAME}:latest
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
    ports:
      - target: 6379
        published: 6379
        protocol: tcp
        mode: ingress
    deploy:
      mode: replicated
      replicas: 1
      restart_policy:
        condition: on-failure

  ${GAME_COORDINATOR_SERVICE_NAME}:
    image: ${REGISTRY_ADDR_OPT}${GAME_COORDINATOR_IMAGE_NAME}:latest
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
    image: ${REGISTRY_ADDR_OPT}${DEMIGOD_IMAGE_NAME}:latest
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
				{"GAME_SERVER_IMAGE_NAME", _GAME_SERVER_IMAGE_NAME},
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
DOCKER_FILE_DEF BUILD_ATLASNET_SRC = R"DOCKER(
RUN mkdir build
RUN --mount=type=cache,target=${WORKDIR}/build \
    cmake -S . -B ${WORKDIR}/build -DCMAKE_BUILD_TYPE=Debug

RUN --mount=type=cache,target=${WORKDIR}/build \
    cmake --build ${WORKDIR}/build --parallel --target ${BUILD_PROJECT}

RUN --mount=type=cache,target=${WORKDIR}/build \
    cmake --install ${WORKDIR}/build --component ${BUILD_PROJECT} --prefix ${WORKDIR}/bin

RUN --mount=type=cache,target=${WORKDIR}/build \
    mkdir -p "${WORKDIR}/deps" && \
    cd "${WORKDIR}/build/vcpkg_installed" && \
    find . -type f -name '*.so*' | while read -r f; do \
        base=$(basename "$f"); \
        cp "$f" "${WORKDIR}/deps/$base"; \
    done


)DOCKER";

DOCKER_FILE_DEF GodDockerFile = MacroParse(
	Generic_Builder_Header + GET_REQUIRED_BUILD_PKGS + VCPKG_Install + COPY_ATLASNET_SRC +
		BUILD_ATLASNET_SRC + Generic_Run_Header + GET_REQUIRED_RUN_PKGS + CopyBuild_StripLib +
		R"(ENTRYPOINT ["${WORKDIR}/bin/God"])",
	{{"OS_VERSION", _DOCKER_OS_}, {"WORKDIR", _DOCKER_WORKDIR_}, {"BUILD_PROJECT", "God"}});

DOCKER_FILE_DEF DemiGodDockerFile = MacroParse(
	Generic_Builder_Header + GET_REQUIRED_BUILD_PKGS + VCPKG_Install + COPY_ATLASNET_SRC +
		BUILD_ATLASNET_SRC + Generic_Run_Header + GET_REQUIRED_RUN_PKGS + CopyBuild_StripLib +
		R"(ENTRYPOINT ["${WORKDIR}/bin/DemiGod"])",
	{{"OS_VERSION", _DOCKER_OS_}, {"WORKDIR", _DOCKER_WORKDIR_}, {"BUILD_PROJECT", "DemiGod"}});

DOCKER_FILE_DEF ClusterDBDockerFile = MacroParse(
	Generic_Builder_Header + GET_REQUIRED_BUILD_PKGS + VCPKG_Install + COPY_ATLASNET_SRC +
		BUILD_ATLASNET_SRC + Generic_Run_Header + GET_REQUIRED_RUN_PKGS + CopyBuild_StripLib +
		R"(ENTRYPOINT ["${WORKDIR}/bin/ClusterDB"])",
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

DOCKER_FILE_DEF PartitionSuperVisordConf = R"(
[supervisord]
nodaemon=true
user=root

[group:atlasnet]
programs=partition,game_server
priority=10

[program:partition]
command=/bin/sh -lc 'set -e; /atlasnet/bin/Partition; rc=$?; [ $rc -eq 0 ] || kill -TERM 1; exit $rc'
autostart=true
autorestart=false
startretries=0
stopasgroup=true
killasgroup=true
stdout_logfile=/dev/stdout
stdout_logfile_maxbytes=0
stderr_logfile=/dev/stderr
stderr_logfile_maxbytes=0

[program:game_server]
command=/bin/sh -lc 'set -e; exec ${GAMESERVER_RUN_COMMAND}; rc=$?; [ $rc -eq 0 ] || kill -TERM 1; exit $rc'
autostart=true
autorestart=false
startretries=0
stopasgroup=true
killasgroup=true
stdout_logfile=/dev/stdout
stdout_logfile_maxbytes=0
stderr_logfile=/dev/stderr
stderr_logfile_maxbytes=0
)";

DOCKER_FILE_DEF PartitionEntryPoint = R"(
  # Create supervisord config file

  ENTRYPOINT ["/usr/bin/supervisord", "-c", "${WORKDIR}/supervisord.conf"]
  
  )";