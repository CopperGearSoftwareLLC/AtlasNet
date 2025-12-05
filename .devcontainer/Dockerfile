FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    # Init / quality-of-life
    tini less coreutils \
    \
    # Networking / server debugging
    iproute2 iputils-ping net-tools dnsutils tcpdump nmap socat \
    curl wget jq openssh-client \
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
    docker.io iptables uidmap docker-buildx \
    && rm -rf /var/lib/apt/lists/*


# Allow vscode user to use docker CLI (dockerd runs as root under supervisor)
RUN usermod -aG docker vscode

# Put our supervisor + startup scripts in place
COPY supervisord.conf /etc/supervisor/conf.d/supervisord.conf
COPY startup.sh /usr/local/bin/startup.sh
RUN chmod +x /usr/local/bin/startup.sh

EXPOSE 3000 5000 8000 8080 8443 9443 5901 6080

CMD ["/usr/bin/supervisord", "-n"]
