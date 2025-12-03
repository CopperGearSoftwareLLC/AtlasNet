#!/usr/bin/env bash
# @desc: copy and paste client build to wsl path.
# @category: general
# ==========================================

# --- CONFIGURATION ---

# Source: The output folder from the Unity build (on D: drive mounted in WSL)
SOURCE_DIR="/mnt/d/KDNet/KDnetUnity/Linux/BuildClient"

# Destination: Your local WSL Desktop folder
# Note: \\wsl.localhost\Ubuntu\home\danny maps directly to /home/danny inside WSL
DEST_DIR="/home/danny/Desktop/BuildClient"

# --- VALIDATION ---

if [ ! -d "$SOURCE_DIR" ]; then
    echo "❌ Error: Source build directory not found!"
    echo "   Checked: $SOURCE_DIR"
    echo "   Did you run build_linux.sh first?"
    exit 1
fi

# --- DEPLOYMENT ---

echo "-----------------------------------"
echo "🚀 Starting Deployment"
echo "   Source:      $SOURCE_DIR"
echo "   Destination: $DEST_DIR"
echo "-----------------------------------"

# 1. Clean the destination to ensure no stale files remain from previous versions
if [ -d "$DEST_DIR" ]; then
    echo "Cleaning old files at destination..."
    rm -rf "$DEST_DIR"
fi

# 2. Re-create the destination folder
mkdir -p "$DEST_DIR"

# 3. Copy files
echo "Copying files..."
# -r: recursive
# -v: verbose (shows what is being copied)
cp -rv "$SOURCE_DIR/." "$DEST_DIR/"

# --- COMPLETION ---

if [ $? -eq 0 ]; then
    echo "-----------------------------------"
    echo "✅ Deployment Complete!"
    echo "   Files are now at: $DEST_DIR"
    echo "-----------------------------------"
    
    # Optional: Make the new binary executable just in case
    # Assuming the executable name matches the one in BuildScript.cs
    chmod +x "$DEST_DIR/KDNetClient.x86_64"
else
    echo "❌ Copy failed."
    exit 1
fi