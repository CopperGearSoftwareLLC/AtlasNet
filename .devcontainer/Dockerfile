FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    tini less coreutils \
    iproute2 iputils-ping net-tools dnsutils tcpdump nmap socat \
    curl wget jq openssh-client \
    build-essential g++ binutils \
    clang clangd \
    cmake ninja-build pkg-config ccache \
    autoconf automake libtool m4 \
    gdb gdbserver git \
    tar zip unzip \
    uuid-dev \
    \
    # Web desktop + noVNC (TigerVNC provides the VNC server)
    fluxbox xterm novnc websockify supervisor \
    tigervnc-standalone-server x11-utils \
    \
    # X11 + OpenGL (headers + runtime/mesa)
    xorg-dev libxmu-dev libxi-dev libxinerama-dev libxcursor-dev \
    libgl-dev libglu1-mesa-dev mesa-utils libgl1-mesa-dri libglx-mesa0 \
    \
    # DinD
    docker.io iptables uidmap docker-buildx \

    # Hotspot
    hotspot linux-tools-generic linux-cloud-tools-generic \

    # Java
    openjdk-21-jdk\

    # NPM
    npm nodejs libnode-dev\
    swig \
     cmake \
    git \
    curl \
    pkg-config \
    libssl-dev \
    zlib1g-dev \
    libglm-dev \
    nlohmann-json3-dev \
    python3 \
    python3-pip \
    unzip \
    wget \
    ninja-build \
    protobuf-compiler \
    libprotobuf-dev \
    libcurl4-openssl-dev \
    autoconf \
    libtool \
    libuv1 \
    libuv1-dev \
    && rm -rf /var/lib/apt/lists/*

RUN usermod -aG docker vscode

COPY supervisord.conf /etc/supervisor/conf.d/supervisord.conf
COPY startup.sh /usr/local/bin/startup.sh
RUN chmod +x /usr/local/bin/startup.sh

EXPOSE 3000 5000 8000 8080 8443 9443 5901 6080
RUN mkdir /tmp/atlasnet-perf
CMD ["/usr/bin/supervisord", "-n"]
