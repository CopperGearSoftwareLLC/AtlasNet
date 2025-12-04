#!/usr/bin/env bash
# @desc: AtlasNetStart and paste plugin to unity.
# @category: atlasnet
# ==========================================

set -e

echo "[INFO] Starting Premake + Deploy pipeline..."

# ================================
# PATHS (CORRECTED)
# ================================
ROOT_DIR="/mnt/d/KDNet/KDNet"
PREMAKE_EXE="$ROOT_DIR/premake5"
PREMAKE_ARG="AtlasNetStart"

SRC_LIB="$ROOT_DIR/bin/DebugDocker/AtlasClientBridge/libAtlasClientBridge.so"
UNITY_PLUGIN_DIR="/mnt/d/KDNet/KDnetUnity/Linux/Assets/Plugins/Linux/x86_64"

# ================================
# RUN PREMAKE5 (cd into root)
# ================================
echo "[INFO] Running Premake5 from project root..."

(
    cd "$ROOT_DIR"
    "$PREMAKE_EXE" "$PREMAKE_ARG"
)

echo "[INFO] Premake5 generation complete!"

# ================================
# COPY LIB TO UNITY PLUGIN FOLDER
# ================================
if [[ ! -f "$SRC_LIB" ]]; then
    echo "[ERROR] Source library not found:"
    echo "       $SRC_LIB"
    exit 1
fi

echo "[INFO] Copying libAtlasClientBridge.so to Unity plugin folder..."

mkdir -p "$UNITY_PLUGIN_DIR"
cp "$SRC_LIB" "$UNITY_PLUGIN_DIR"

echo "[INFO] Deployment complete!"
echo "[INFO] Plugin installed at:"
echo "       $UNITY_PLUGIN_DIR/libAtlasClientBridge.so"

read -p "Press ENTER to exit..."
