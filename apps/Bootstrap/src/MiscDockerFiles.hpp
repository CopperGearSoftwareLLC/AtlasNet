#pragma once
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