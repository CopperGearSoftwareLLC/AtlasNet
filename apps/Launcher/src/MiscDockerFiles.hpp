#pragma once
#include <filesystem>
#include "misc/String_utils.hpp"
#define DOCKER_FILE_DEF const static inline std::string

DOCKER_FILE_DEF GET_REQUIRED_BUILD_PKGS = R"(
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Init / quality-of-life
    tini less coreutils \
    \
    # Networking / server debugging
    iproute2 iputils-ping net-tools dnsutils tcpdump nmap socat \
    curl wget jq openssh-client libssl-dev \
    \
    # Build toolchain (general)
    build-essential g++ binutils \
    clang clangd\
    cmake ninja-build pkg-config ccache \
    autoconf automake libtool m4 \
    \
    # Debuggers / tooling
    gdb gdbserver git \
    \
    # Archives
    tar zip unzip \
    \
    # Dev headers / misc
    uuid-dev \
    \
    # Web desktop (Xvfb + VNC + noVNC)
    xvfb fluxbox xterm x11vnc novnc websockify supervisor \
    \
    # X11 + OpenGL (headers + runtime/mesa)
    xorg-dev libxmu-dev libxi-dev libxinerama-dev libxcursor-dev \
    libgl-dev libglu1-mesa-dev mesa-utils libgl1-mesa-dri libglx-mesa0 \
    \
    # DinD
    docker.io iptables uidmap \
    && rm -rf /var/lib/apt/lists/*
)";
DOCKER_FILE_DEF GET_REQUIRED_RUN_PKGS = R"(
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Init / quality-of-life
    binutils supervisor tini \
    && rm -rf /var/lib/apt/lists/*
)";
DOCKER_FILE_DEF VCPKG_Install = R"(

ARG DEBIAN_FRONTEND=noninteractive
ARG VCPKG_ROOT=/opt/vcpkg
ARG VCPKG_COMMIT= # optional: pin to a commit for reproducibility

ENV VCPKG_ROOT=${VCPKG_ROOT}
ENV PATH="${VCPKG_ROOT}:${PATH}"

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates curl git unzip tar zip cmake ninja-build \
    build-essential pkg-config \
 && rm -rf /var/lib/apt/lists/*

# Install vcpkg
RUN git clone https://github.com/microsoft/vcpkg.git "${VCPKG_ROOT}" \
 && if [ -n "${VCPKG_COMMIT}" ]; then cd "${VCPKG_ROOT}" && git checkout "${VCPKG_COMMIT}"; fi \
 && "${VCPKG_ROOT}/bootstrap-vcpkg.sh" -disableMetrics

# Optional: make CMake pick up vcpkg toolchain automatically if you want
# ENV CMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
# ENV VCPKG_DEFAULT_TRIPLET="x64-linux"
COPY vcpkg.json ./vcpkg.json
RUN vcpkg install
)";
DOCKER_FILE_DEF CopyBuild_StripLib = R"(
COPY --from=builder ${WORKDIR}/bin/ .
COPY --from=builder ${WORKDIR}/deps/*.so /usr/local/lib
RUN ldconfig
)";

DOCKER_FILE_DEF REGISTRY_STACK =
	MacroParse(R"(
version: "3.8"

services:
  registry:
    image: registry:2
    networks:
       ${ATLASNET_NETWORK_NAME}:
        aliases:
          - registry
          - registry-registry
    ports:
      - "5000:5000"
    environment:
      REGISTRY_HTTP_ADDR: "0.0.0.0:5000"
    deploy:
      replicas: 1
      placement:
        constraints:
          - node.role == manager
  
  registry_proxy:
    image: nginx:alpine
    networks:
      ${ATLASNET_NETWORK_NAME}:
        aliases:
          - registry-proxy
          - registry-registry-proxy
    ports:
      - "443:443"  # HTTPS for external world
    volumes:
        - ${NGINX_CONFIG_PATH}:/etc/nginx/conf.d/default.conf:ro

    secrets:
      - source: ${REGISTRY_CERT_SECRET_NAME}
        target: /etc/nginx/server.crt
      - source: ${REGISTRY_KEY_SECRET_NAME}
        target: /etc/nginx/server.key
    deploy:
      replicas: 1

networks:
  ${ATLASNET_NETWORK_NAME}:
    external: true
    name: ${ATLASNET_NETWORK_NAME}

secrets:
  ${REGISTRY_CERT_SECRET_NAME}:
    external: true
  ${REGISTRY_KEY_SECRET_NAME}:
    external: true
)",
			   {{"ATLASNET_NETWORK_NAME", _ATLASNET_NETWORK_NAME},
				{"REGISTRY_KEY_SECRET_NAME", _REGISTRY_KEY_SECRET_NAME},
				{"REGISTRY_CERT_SECRET_NAME", _REGISTRY_CERT_SECRET_NAME},
				{"NGINX_CONFIG_PATH", std::filesystem::absolute(std::string("./")+_DOCKER_TEMP_FILES_DIR + std::string("nginx.conf"))}});

DOCKER_FILE_DEF NGINX_CONFIG = MacroParse(R"(
server {
    listen 443 ssl;

    ssl_certificate /etc/nginx/server.crt;
    ssl_certificate_key /etc/nginx/server.key;

    # Allow large uploads
    client_max_body_size 0;

    location / {
        proxy_pass https://registry:5000;

        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto https;
    }
}
)",
										  {});