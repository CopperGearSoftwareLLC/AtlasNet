#!/usr/bin/env bash
# @desc: complete script to go from atlasnetstart to wsl client executable.
# @category: AtlasNet/Unity
# ==========================================


# ==========================================
#      FULL BUILD PIPELINE: BRIDGE -> UNITY -> DEPLOY
# ==========================================
# This script combines logic from:
# 1. deploy_bridge.sh
# 2. create_unity_build_client_linux.sh
# 3. deploy_unity_to_wsl.sh

set -e # Exit immediately if any command fails

# --- GLOBAL CONFIGURATION ---

# 1. AtlasNet / Bridge Paths
BRIDGE_ROOT="/mnt/d/KDNet/KDNet"
BRIDGE_PREMAKE_EXE="$BRIDGE_ROOT/premake5"
BRIDGE_PREMAKE_ARG="AtlasNetStart"
BRIDGE_SRC_LIB="$BRIDGE_ROOT/bin/DebugDocker/AtlasClientBridge/libAtlasClientBridge.so"

# 2. Unity Project Paths
UNITY_PROJECT_PATH="/mnt/d/KDNet/KDnetUnity/Linux"
UNITY_PLUGIN_DIR="$UNITY_PROJECT_PATH/Assets/Plugins/Linux/x86_64"
UNITY_BUILD_OUTPUT_DIR="$UNITY_PROJECT_PATH/BuildClient"
UNITY_EXE_PATH="/mnt/c/Program Files/Unity/Hub/Editor/6000.0.60f1/Editor/Unity.exe"
UNITY_LOG_FILE="$(pwd)/build.log"

# 3. Final Deployment Paths
DEPLOY_DEST_DIR="/home/danny/Desktop/BuildClient"


# ==============================================================================
# STEP 1: BUILD BRIDGE & DEPLOY TO UNITY (from deploy_bridge.sh)
# ==============================================================================
echo "--------------------------------------------------------"
echo "🔧 [STEP 1/3] Building AtlasNet Bridge & Copying to Unity"
echo "--------------------------------------------------------"

if [ ! -d "$BRIDGE_ROOT" ]; then
    echo "❌ Error: Bridge root not found at $BRIDGE_ROOT"
    exit 1
fi

echo "   Running Premake5 ($BRIDGE_PREMAKE_ARG)..."
# Run inside a subshell to avoid changing global directory
(
    cd "$BRIDGE_ROOT"
    "$BRIDGE_PREMAKE_EXE" "$BRIDGE_PREMAKE_ARG"
)

# Verify the .so was generated/exists
if [[ ! -f "$BRIDGE_SRC_LIB" ]]; then
    echo "❌ Error: Source library not found at: $BRIDGE_SRC_LIB"
    exit 1
fi

echo "   Copying plugin to: $UNITY_PLUGIN_DIR"
mkdir -p "$UNITY_PLUGIN_DIR"
cp "$BRIDGE_SRC_LIB" "$UNITY_PLUGIN_DIR"
echo "✅ Bridge deployed successfully."


# ==============================================================================
# STEP 2: BUILD UNITY LINUX CLIENT (from create_unity_build_client_linux.sh)
# ==============================================================================
echo "--------------------------------------------------------"
echo "🏗️  [STEP 2/3] Building Unity Linux Client"
echo "--------------------------------------------------------"

# Validation
if [ ! -f "$UNITY_EXE_PATH" ]; then
    echo "❌ Error: Unity executable not found at: $UNITY_EXE_PATH"
    echo "   Note: In WSL, drives are mounted at /mnt/c/"
    exit 1
fi

if [ ! -d "$UNITY_PROJECT_PATH/Assets" ]; then
    echo "❌ Error: Unity Project Assets not found at: $UNITY_PROJECT_PATH"
    exit 1
fi

echo "   Cleaning previous Unity build output..."
rm -rf "$UNITY_BUILD_OUTPUT_DIR"
mkdir -p "$UNITY_BUILD_OUTPUT_DIR"

# Path Conversions for Windows Unity.exe
WINDOWS_PROJECT_PATH=$(wslpath -m "$UNITY_PROJECT_PATH")
WINDOWS_LOG_PATH=$(wslpath -m "$UNITY_LOG_FILE")

echo "   Project (Win): $WINDOWS_PROJECT_PATH"
echo "   Log File:      $UNITY_LOG_FILE"

echo "   Starting Unity in batchmode..."
"$UNITY_EXE_PATH" \
  -quit \
  -batchmode \
  -nographics \
  -projectPath "$WINDOWS_PROJECT_PATH" \
  -executeMethod BuildScript.BuildLinux \
  -logFile "$WINDOWS_LOG_PATH"

# Unity doesn't always return standard exit codes in batchmode, 
# but we check if the build folder has content as a secondary verify.
if [ -z "$(ls -A "$UNITY_BUILD_OUTPUT_DIR")" ]; then
     echo "❌ Build Failed! Output directory is empty."
     echo "--- UNITY LOG ---"
     cat "$UNITY_LOG_FILE"
     exit 1
fi

echo "✅ Unity Build finished."


# ==============================================================================
# STEP 3: DEPLOY TO WSL DESKTOP (from deploy_unity_to_wsl.sh)
# ==============================================================================
echo "--------------------------------------------------------"
echo "🚀 [STEP 3/3] Deploying to WSL Desktop"
echo "--------------------------------------------------------"

echo "   Source:      $UNITY_BUILD_OUTPUT_DIR"
echo "   Destination: $DEPLOY_DEST_DIR"

if [ -d "$DEPLOY_DEST_DIR" ]; then
    echo "   Cleaning old deployment..."
    rm -rf "$DEPLOY_DEST_DIR"
fi

mkdir -p "$DEPLOY_DEST_DIR"

echo "   Copying build files..."
cp -r "$UNITY_BUILD_OUTPUT_DIR/." "$DEPLOY_DEST_DIR/"

# Make executable (Safety check)
chmod +x "$DEPLOY_DEST_DIR/KDNetClient.x86_64" 2>/dev/null || true

echo "--------------------------------------------------------"
echo "✨ PIPELINE COMPLETE!"
echo "   Executable: $DEPLOY_DEST_DIR/KDNetClient.x86_64"
echo "--------------------------------------------------------"

read -p "Press ENTER to exit..."