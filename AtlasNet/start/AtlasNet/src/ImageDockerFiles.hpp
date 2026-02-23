#pragma once

#include <string>

#include "Misc/String_utils.hpp"
#include "MiscDockerFiles.hpp"
#include "Utils.hpp"
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
      replicas: 1
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
    image: valkey/valkey:latest
    command: ["valkey-server", "--appendonly", "yes", "--port", "${INTERNAL_REDIS_PORT}"]
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
      - "3000:3000"   # Next.js default
      - "9229:9229"   # Node inspector
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
      - target: ${PORT_PROXY}
        published: ${PORT_PROXY_PUBLISHED}
        protocol: tcp
        mode: ingress
      
      - target: ${PORT_PROXY}
        published: ${PORT_PROXY_PUBLISHED}
        protocol: udp
        mode: ingress

    deploy:
      mode: replicated
      replicas: 1
      restart_policy:
        condition: on-failure
        
${BUILTIN_DB_STACK}

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
	MacroParse(ATLASNET_STACK_RAW, {{"WATCHDOG_IMAGE_NAME", _WATCHDOG_IMAGE_NAME},
									{"WATCHDOG_SERVICE_NAME", _WATCHDOG_SERVICE_NAME},
									{"ATLASNET_NETWORK_NAME", _ATLASNET_NETWORK_NAME},
									{"REGISTRY_ADDR", ""},	//"localhost/"
									{"GAME_SERVER_IMAGE_NAME", _GAME_SERVER_IMAGE_NAME},
									{"SHARD_SERVICE_NAME", _SHARD_SERVICE_NAME},
									{"CARTOGRAPH_IMAGE_NAME", _CARTOGRAPH_IMAGE_NAME},
									{"CARTOGRAPH_SERVICE_NAME", _CARTOGRAPH_SERVICE_NAME},
									{"PROXY_IMAGE_NAME", _PROXY_IMAGE_NAME},
									{"PROXY_SERVICE_NAME", _PROXY_SERVICE_NAME},
                  {"PORT_PROXY",std::to_string(_PORT_PROXY)},
                  {"PORT_PROXY_PUBLISHED",std::to_string(_PORT_PROXY_PUBLISHED)},
									{"INTERNAL_REDIS_SERVICE_NAME", _INTERNAL_REDIS_SERVICE_NAME},
									{"INTERNAL_REDIS_PORT", std::to_string(_INTERNAL_REDIS_PORT)}});
DOCKER_FILE_DEF Generic_Builder_Header = R"(
# syntax=docker/dockerfile:1.4
FROM ${OS_VERSION} AS builder
WORKDIR ${WORKDIR}

)";

DOCKER_FILE_DEF Generic_Run_Header = R"(
FROM ${OS_VERSION} AS runner
WORKDIR ${WORKDIR}
)";
DOCKER_FILE_DEF SETUP_ATLASNET_DEPS = R"(
COPY --from=atlasnetsdk ./vcpkg.json ./vcpkg.json

ENV VCPKG_ROOT=/opt/vcpkg/
# Create the directory
RUN mkdir -p $VCPKG_ROOT

# Download the compressed release, extract to /opt/vcpkg, and remove the tar to save space
RUN curl -L https://github.com/microsoft/vcpkg/archive/refs/tags/2026.01.16.tar.gz \
| tar -xz --strip-components=1 -C ${VCPKG_ROOT}
RUN  /opt/vcpkg/bootstrap-vcpkg.sh 
RUN apt-get update && apt-get install binutils build-essential -y
RUN --mount=type=cache,target=${WORKDIR}/build/vcpkg_installed VCPKG_MANIFEST_MODE=ON ${VCPKG_ROOT}/vcpkg install \
       --x-install-root=${WORKDIR}/build/vcpkg_installed
)";
DOCKER_FILE_DEF COPY_ATLASNET_SRC = R"(

COPY --from=atlasnetsdk . ./.
)";
DOCKER_FILE_DEF BUILD_ATLASNET_SRC = R"DOCKER(
#--mount=type=cache,target=${WORKDIR}/build \
ENV CC=clang
ENV CXX=clang++
#--mount=type=cache,target=${WORKDIR}/build/vcpkg_installed


RUN --mount=type=cache,target=${WORKDIR}/build/vcpkg_installed cmake -S . -B ${WORKDIR}/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
 -DATLASNET_INCLUDE_RUNTIME=ON -DATLASNET_INCLUDE_LIBS=ON -DVCPKG_MANIFEST_MODE=ON \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_INSTALLED_DIR=${WORKDIR}/build/vcpkg_installed -DATLASNET_INCLUDE_WEB=OFF ${CMAKE_ARGS}
RUN  --mount=type=cache,target=${WORKDIR}/build/vcpkg_installed cmake --build ${WORKDIR}/build --parallel --target BuiltInDB Database Debug Docker Entity Events Heuristic Interlink InternalDB

RUN --mount=type=cache,target=${WORKDIR}/build/vcpkg_installed  cmake --build ${WORKDIR}/build --parallel --target ${BUILD_PROJECT}


RUN --mount=type=cache,target=${WORKDIR}/build/vcpkg_installed cmake --install ${WORKDIR}/build --component ${BUILD_PROJECT} --prefix ${WORKDIR}/bin


# 8RUN mkdir -p /usr/local/lib && \
# 8    find /deps -maxdepth 4 -type f -name '*.so*' | while read -r f; do \
# 8        cp "$f" /usr/local/lib/; \
# 8    done && \
# 8    # Update the dynamic linker cache
# 8    ldconfig

)DOCKER";

DOCKER_FILE_DEF WatchDogDockerFile = MacroParse(
	Generic_Builder_Header + GET_REQUIRED_BUILD_PKGS + InstallDeps + SETUP_ATLASNET_DEPS +
		COPY_ATLASNET_SRC + BUILD_ATLASNET_SRC + Generic_Run_Header + GET_REQUIRED_RUN_PKGS +
		CopyBuild_StripLib + R"(ENTRYPOINT ["${WORKDIR}/bin/watchdog"])",
	{{"OS_VERSION", _DOCKER_OS_},
	 {"WORKDIR", _DOCKER_WORKDIR_},
	 {"BUILD_PROJECT", "watchdog"},
	 {"CMAKE_ARGS", ""}});

DOCKER_FILE_DEF ProxyDockerFile = MacroParse(
	Generic_Builder_Header + GET_REQUIRED_BUILD_PKGS + InstallDeps + SETUP_ATLASNET_DEPS +
		COPY_ATLASNET_SRC + BUILD_ATLASNET_SRC + Generic_Run_Header + GET_REQUIRED_RUN_PKGS +
		CopyBuild_StripLib + R"(ENTRYPOINT ["${WORKDIR}/bin/proxy"])",
	{{"OS_VERSION", _DOCKER_OS_},
	 {"WORKDIR", _DOCKER_WORKDIR_},
	 {"BUILD_PROJECT", "proxy"},
	 {"CMAKE_ARGS", ""}});

DOCKER_FILE_DEF ShardDockerFile =
	MacroParse(Generic_Builder_Header + GET_REQUIRED_BUILD_PKGS + InstallDeps + SETUP_ATLASNET_DEPS +
				   COPY_ATLASNET_SRC + BUILD_ATLASNET_SRC + Generic_Run_Header +
				   GET_REQUIRED_RUN_PKGS + CopyBuild_StripLib,
			   {{"OS_VERSION", _DOCKER_OS_},
				{"WORKDIR", _DOCKER_WORKDIR_},
				{"BUILD_PROJECT", "shard"},
				{"CMAKE_ARGS", ""}});

DOCKER_FILE_DEF CartographDockerFile = MacroParse(
	Generic_Builder_Header + GET_REQUIRED_BUILD_PKGS  +
		InstallDeps + SETUP_ATLASNET_DEPS +
		R"(RUN apt-get update && apt-get install -y --no-install-recommends \
    swig npm nodejs libnode-dev \
    && rm -rf /var/lib/apt/lists/*)"+ COPY_ATLASNET_SRC + BUILD_ATLASNET_SRC +
		Generic_Run_Header + GET_REQUIRED_RUN_PKGS + R"(WORKDIR ${WORKDIR}/web
ENV NODE_ENV=development
RUN apt update \
 && apt install -y curl ca-certificates \
 && curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
 && apt install -y nodejs \
 && node -v \
 && npm -v
# Copy built app + node_modules
COPY --from=atlasnetsdk:latest runtime/cartograph/web/. ./
RUN  npm install
COPY --from=builder ${WORKDIR}/runtime/cartograph/web/nextjs ./nextjs/
#COPY --from=builder ${WORKDIR}/runtime/cartograph/web ./
#RUN rm -rf ./node_modules ./.next ./native-server/node_modules
# RUN npm run build
# Next.js default port
EXPOSE 3000
#EXPOSE 9229  # optional if you want debugging

# Start your Next.js app using npm run dev

CMD ["npm", "run", "dev:all"]
)",
	{{"OS_VERSION", _DOCKER_OS_},
	 {"WORKDIR", _DOCKER_WORKDIR_},
	 {"BUILD_PROJECT", "cartograph_web_native_deps"},
	 {"CMAKE_ARGS", "-DATLASNET_INCLUDE_WEB=ON -DENABLE_NODE=ON -DATLASNET_INCLUDE_RUNTIME=ON"}});

DOCKER_FILE_DEF ShardSuperVisordConf = R"(
[supervisord]
nodaemon=true

[program:shard]
command=${WORKDIR}/bin/shard
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
	 {"SHARD_IMAGE", "" + std::string(_SHARD_IMAGE_NAME) + ""},
	 {"WORKDIR", _DOCKER_WORKDIR_},
	 {"SUPERVISORD_CONF", ShardSuperVisordConf}});