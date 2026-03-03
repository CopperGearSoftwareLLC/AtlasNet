FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

ENV DEBIAN_FRONTEND=noninteractive

# disabled cuz was failing to build dev container on arm64
# RUN apt-get update && apt-get install -y --no-install-recommends \
RUN set -eux; \
    # Some networks block plain HTTP; Ubuntu arm64 uses ports.ubuntu.com by default.
    # Switch apt sources to HTTPS so apt-get update can succeed.
    if [ -f /etc/apt/sources.list.d/ubuntu.sources ]; then \
      sed -i 's|http://ports.ubuntu.com/ubuntu-ports|https://ports.ubuntu.com/ubuntu-ports|g' /etc/apt/sources.list.d/ubuntu.sources; \
    fi; \
    if [ -f /etc/apt/sources.list ]; then \
      sed -i 's|http://ports.ubuntu.com/ubuntu-ports|https://ports.ubuntu.com/ubuntu-ports|g' /etc/apt/sources.list; \
    fi; \
    apt-get -o Acquire::Retries=3 update; \
    apt-get install -y --no-install-recommends \
    tini less coreutils acl \
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
    # Docker CLI / Buildx (host daemon via mounted socket)
    docker.io docker-buildx \
    \
    # Hotspot
    hotspot linux-tools-generic \
    \
    # Java
    openjdk-21-jdk \
    \
    # NPM
    npm nodejs libnode-dev \
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
