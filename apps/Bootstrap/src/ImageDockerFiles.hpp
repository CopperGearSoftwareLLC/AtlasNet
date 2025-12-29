#pragma once

#include "MiscDockerFiles.hpp"
#include "Misc/String_utils.hpp"

DOCKER_FILE_DEF ATLASNET_STACK_RAW = R"(
version: "3.8"

services:
  ${WATCHDOG_SERVICE_NAME}:
    image: ${REGISTRY_ADDR_OPT}${WATCHDOG_IMAGE_NAME}
    networks: [${ATLASNET_NETWORK_NAME}]
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock
    deploy:
      placement:
          constraints:
            - 'node.role == manager'
      mode: replicated
      replicas: ${WATCHDOG_REPLICA_NUM}
      restart_policy:
        condition: on-failure

  ${SHARD_SERVICE_NAME}:
    image: ${REGISTRY_ADDR_OPT}${GAME_SERVER_IMAGE_NAME}:latest
    networks: [${ATLASNET_NETWORK_NAME}]
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock
    deploy:
     resources:
        limits:
          cpus: "1.0"      # 1 core
          memory: 1G       # 1 GiB
     mode: replicated
     replicas: 0   # 0 replicas as requested (exists, but wont run)
     restart_policy:
       condition: on-failure

  ${INTERNAL_REDIS_SERVICE_NAME}:
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
  ${CARTOGRAPH_SERVICE_NAME}:
    image: ${CARTOGRAPH_IMAGE_NAME}
    networks: [${ATLASNET_NETWORK_NAME}]
    ports:
      - target: 3000
        published: 3000
        protocol: tcp
        mode: ingress
    deploy:
      placement:
          constraints:
            - 'node.role == manager'
      mode: replicated
      replicas: 1
      restart_policy:
        condition: on-failure


  ${PROXY_SERVICE_NAME}:
    image: ${REGISTRY_ADDR_OPT}${PROXY_IMAGE_NAME}:latest
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
/*
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
		condition: on-failure*/
DOCKER_FILE_DEF ATLASNET_STACK =
	MacroParse(ATLASNET_STACK_RAW,
			   {{"WATCHDOG_IMAGE_NAME", _WATCHDOG_IMAGE_NAME},
				{"WATCHDOG_SERVICE_NAME", _WATCHDOG_SERVICE_NAME},
				{"ATLASNET_NETWORK_NAME", _ATLASNET_NETWORK_NAME},
				{"WATCHDOG_REPLICA_NUM", std::to_string(1)},
				{"REGISTRY_ADDR", ""},	//"localhost/"
				{"GAME_SERVER_IMAGE_NAME", _GAME_SERVER_IMAGE_NAME},
				{"SHARD_SERVICE_NAME", _SHARD_SERVICE_NAME},
				{"CARTOGRAPH_IMAGE_NAME", _CARTOGRAPH_IMAGE_NAME},
				{"CARTOGRAPH_SERVICE_NAME", _CARTOGRAPH_SERVICE_NAME},
				{"PROXY_IMAGE_NAME", _PROXY_IMAGE_NAME},
				{"PROXY_SERVICE_NAME", _PROXY_SERVICE_NAME},
				{"INTERNAL_REDIS_SERVICE_NAME", _INTERNAL_REDIS_SERVICE_NAME}});
DOCKER_FILE_DEF Generic_Builder_Header = R"(
FROM ${OS_VERSION} AS builder
WORKDIR ${WORKDIR}

)";

DOCKER_FILE_DEF Generic_Run_Header = R"(
FROM ${OS_VERSION} AS runner
WORKDIR ${WORKDIR}
)";

DOCKER_FILE_DEF COPY_ATLASNET_SRC =
	MacroParse(R"(
COPY ${BOOTSTRAP_RUNTIME_SRC_DIR}/. ./
)",
			   {{"BOOTSTRAP_RUNTIME_SRC_DIR", BOOTSTRAP_RUNTIME_SRC_DIR}});
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

DOCKER_FILE_DEF WatchDogDockerFile = MacroParse(
	Generic_Builder_Header + GET_REQUIRED_BUILD_PKGS + VCPKG_Install + COPY_ATLASNET_SRC +
		BUILD_ATLASNET_SRC + Generic_Run_Header + GET_REQUIRED_RUN_PKGS + CopyBuild_StripLib +
		R"(ENTRYPOINT ["${WORKDIR}/bin/WatchDog"])",
	{{"OS_VERSION", _DOCKER_OS_}, {"WORKDIR", _DOCKER_WORKDIR_}, {"BUILD_PROJECT", "WatchDog"}});

DOCKER_FILE_DEF ProxyDockerFile = MacroParse(
	Generic_Builder_Header + GET_REQUIRED_BUILD_PKGS + VCPKG_Install + COPY_ATLASNET_SRC +
		BUILD_ATLASNET_SRC + Generic_Run_Header + GET_REQUIRED_RUN_PKGS + CopyBuild_StripLib +
		R"(ENTRYPOINT ["${WORKDIR}/bin/Proxy"])",
	{{"OS_VERSION", _DOCKER_OS_}, {"WORKDIR", _DOCKER_WORKDIR_}, {"BUILD_PROJECT", "Proxy"}});

DOCKER_FILE_DEF ClusterDBDockerFile = MacroParse(
	Generic_Builder_Header + GET_REQUIRED_BUILD_PKGS + VCPKG_Install + COPY_ATLASNET_SRC +
		BUILD_ATLASNET_SRC + Generic_Run_Header + GET_REQUIRED_RUN_PKGS + CopyBuild_StripLib +
		R"(ENTRYPOINT ["${WORKDIR}/bin/ClusterDB"])",
	{{"OS_VERSION", _DOCKER_OS_}, {"WORKDIR", _DOCKER_WORKDIR_}, {"BUILD_PROJECT", "ClusterDB"}});

DOCKER_FILE_DEF GameCoordinatorDockerFile = MacroParse(
	Generic_Builder_Header + GET_REQUIRED_BUILD_PKGS + VCPKG_Install + COPY_ATLASNET_SRC +
		BUILD_ATLASNET_SRC + Generic_Run_Header + GET_REQUIRED_RUN_PKGS + CopyBuild_StripLib +
		R"(ENTRYPOINT [""])",
	{{"OS_VERSION", _DOCKER_OS_}, {"WORKDIR", _DOCKER_WORKDIR_}, {"BUILD_PROJECT", "WatchDog"}});

DOCKER_FILE_DEF ShardDockerFile = MacroParse(
	Generic_Builder_Header + GET_REQUIRED_BUILD_PKGS + VCPKG_Install + COPY_ATLASNET_SRC +
		BUILD_ATLASNET_SRC + Generic_Run_Header + GET_REQUIRED_RUN_PKGS + CopyBuild_StripLib,
	{{"OS_VERSION", _DOCKER_OS_}, {"WORKDIR", _DOCKER_WORKDIR_}, {"BUILD_PROJECT", "Shard"}});

DOCKER_FILE_DEF ShardSuperVisordConf = R"(
[supervisord]
nodaemon=true

[program:shard]
command=${WORKDIR}/bin/Shard
directory=${WORKDIR}
autorestart=true
stdout_logfile=/dev/stdout
stdout_logfile_maxbytes=0
stderr_logfile=/dev/stderr
stderr_logfile_maxbytes=0

[program:game_server]
command=${GAME_SERVER_RUN_COMMAND}
directory=${GAME_SERVER_WORK_DIR}
autorestart=true
stdout_logfile=/dev/stdout
stdout_logfile_maxbytes=0
stderr_logfile=/dev/stderr
stderr_logfile_maxbytes=0
  )";
;

DOCKER_FILE_DEF GameServerEntryPoint = MacroParse(
	R"(
COPY --from=${SHARD_IMAGE} ${WORKDIR} ${WORKDIR}
ENV LD_LIBRARY_PATH="${WORKDIR}/deps:${LD_LIBRARY_PATH}"
${SHARD_INSTALL_RUNTIME_DEPS}
${WRITE_SUPERVISOR_FILE}
ENTRYPOINT ["/usr/bin/supervisord", "-c", "/supervisord.conf"])",
	{{"WRITE_SUPERVISOR_FILE", R"(
RUN cat > /supervisord.conf <<'EOF'
${SUPERVISORD_CONF}
EOF
)"},
	 {"SHARD_INSTALL_RUNTIME_DEPS", GET_REQUIRED_RUN_PKGS},
	 {"SHARD_IMAGE", _SHARD_IMAGE_NAME},
	 {"WORKDIR", _DOCKER_WORKDIR_},
	 {"SUPERVISORD_CONF", ShardSuperVisordConf}});