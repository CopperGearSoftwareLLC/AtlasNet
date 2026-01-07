#!/usr/bin/env bash
set -e

# Base directories
BASE_DIR="$(pwd)"
DEPS_DIR="$BASE_DIR/deps"

echo "=== AtlasNet Ubuntu dependency installer ==="

mkdir -p "$DEPS_DIR"

# --- System packages (apt) ---
echo "Installing system packages..."
  apt update
  apt install -y \
    build-essential \
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
    libtool

# --- Boost ---
BOOST_VERSION="1.90.0"
BOOST_DIR="$DEPS_DIR/boost-1.90.0"
BOOST_INSTALL_DIR="$DEPS_DIR/boost_install"
BOOST_MODULES=(uuid container stacktrace)
BOOST_TAR="$DEPS_DIR/boost_1_90_0.tar.bz2"
BOOST_URL="https://github.com/boostorg/boost/releases/download/boost-1.90.0/boost-1.90.0-b2-nodocs.tar.gz"

if [ ! -f "$BOOST_TAR" ]; then
    echo "Downloading Boost $BOOST_VERSION..."
    wget -O "$BOOST_TAR" "$BOOST_URL"
fi

if [ ! -d "$BOOST_DIR" ]; then
    echo "Extracting Boost..."
    tar -xzf "$BOOST_TAR" -C "$DEPS_DIR"

fi

cd "$BOOST_DIR"
rm "$BOOST_TAR"

# Convert module array to comma-separated list
BOOST_COMPONENTS=$(IFS=','; echo "${BOOST_MODULES[*]}")

echo "Bootstrapping Boost with modules: $BOOST_COMPONENTS"
./bootstrap.sh --prefix="$BOOST_INSTALL_DIR" --with-libraries="$BOOST_COMPONENTS"

echo "Building and installing Boost..."
./b2 install -j$(nproc) \
    cxxflags="-fPIC" \
    cflags="-fPIC" \
    link=static \
    runtime-link=shared

cd "$BASE_DIR"
rm -rf "$BOOST_DIR"
echo "Boost installed in $BOOST_INSTALL_DIR"



# --- GameNetworkingSockets ---
GNS_DIR="$DEPS_DIR/GameNetworkingSockets"
if [ ! -d "$GNS_DIR" ]; then
    echo "Cloning GameNetworkingSockets..."
    git clone --recursive https://github.com/ValveSoftware/GameNetworkingSockets.git "$GNS_DIR"
fi
cd "$GNS_DIR"
mkdir -p build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX="$DEPS_DIR/GameNetworkingSockets_install" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=OFF
make -j$(nproc)
make install
cd "$BASE_DIR"
rm -rf "$GNS_DIR"

# --- hiredis ---
HIREDIS_DIR="$DEPS_DIR/hiredis"
if [ ! -d "$HIREDIS_DIR" ]; then
    echo "Cloning hiredis..."
    git clone https://github.com/redis/hiredis.git "$HIREDIS_DIR"
fi
cd "$HIREDIS_DIR"
mkdir -p build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DENABLE_SSL=ON \
    -DCMAKE_INSTALL_PREFIX="$DEPS_DIR/hiredis_install"
make -j$(nproc)
make install
cd "$BASE_DIR"
rm -rf "$HIREDIS_DIR"

# --- redis-plus-plus ---
REDIS_PLUS_DIR="$DEPS_DIR/redis-plus-plus"

if [ ! -d "$REDIS_PLUS_DIR" ]; then
    echo "Cloning redis-plus-plus..."
    git clone https://github.com/sewenew/redis-plus-plus.git "$REDIS_PLUS_DIR"
fi

cd "$REDIS_PLUS_DIR"
mkdir -p build && cd build

cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_INSTALL_PREFIX="$DEPS_DIR/redis-plus-plus_install" \
    -DCMAKE_PREFIX_PATH="$DEPS_DIR/hiredis_install" \
    -DREDIS_PLUS_PLUS_BUILD_ASYNC=ON \
    -DREDIS_PLUS_PLUS_BUILD_ASYNC=libuv \
    -DREDIS_PLUS_PLUS_USE_TLS=ON

make -j"$(nproc)"
make install

cd "$BASE_DIR"
rm -rf "$REDIS_PLUS_DIR"
#rm -rf deps
echo "All dependencies installed successfully!"
#echo ""
#echo "To build AtlasNet or examples using these deps, pass CMake:"
#echo "  -DCMAKE_PREFIX_PATH=$DEPS_DIR/GameNetworkingSockets/install;$DEPS_DIR/redis-plus-plus/install;$DEPS_DIR/ftxui/install"
