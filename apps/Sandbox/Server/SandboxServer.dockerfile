
FROM ubuntu:24.04 AS builder
WORKDIR /atlasnet


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

COPY CMakeLists.txt CMakeLists.txt
COPY apps ./apps
COPY libs ./libs

RUN mkdir build
RUN  --mount=type=cache,target=/build \ 
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
RUN  --mount=type=cache,target=/build/apps \
    --mount=type=cache,target=/build/libs \
     cmake --build build --parallel --target SandboxServer 
RUN --mount=type=cache,target=/build \
    cmake --install build --component SandboxServer --prefix /atlasnet/

FROM ubuntu:24.04 AS runner
WORKDIR /atlasnet

RUN apt-get update && apt-get install -y --no-install-recommends \
    # Init / quality-of-life
    binutils \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /atlasnet/bin /atlasnet/

COPY --from=builder /atlasnet/build/vcpkg_installed/x64-linux/lib/*.so /usr/local/lib
RUN ldconfig
#${ATLASNET_PARTITION_INHERIT}
#WORKDIR /atlasnet
#COPY --from=runner /atlasnet/bin /atlasnet/
#COPY --from=runner /atlasnet/build/vcpkg_installed/x64-linux/lib/*.so /usr/local/lib
#RUN ldconfig
