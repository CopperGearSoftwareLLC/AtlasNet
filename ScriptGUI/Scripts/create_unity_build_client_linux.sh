#!/usr/bin/env bash
# @desc: run unity headless and build linux build.
# @category: atlasnet
# ==========================================

# --- CONFIGURATION (WSL VERSION) ---

# 1. Project Path 
# In WSL, D: drive is usually at /mnt/d/
PROJECT_PATH="/mnt/d/KDNet/KDnetUnity/Linux"

# 2. Path to your Unity Editor Executable
# In WSL, C: drive is usually at /mnt/c/
UNITY_PATH="/mnt/c/Program Files/Unity/Hub/Editor/6000.0.60f1/Editor/Unity.exe"

# 3. Log file location (Absolute Linux Path)
LOG_FILE_LINUX="$(pwd)/build.log"

# --- VALIDATION ---

# Check if Unity exists
if [ ! -f "$UNITY_PATH" ]; then
    echo "Error: Unity executable not found at: $UNITY_PATH"
    echo "NOTE: In WSL, drives are usually mounted at /mnt/c/ rather than /c/"
    exit 1
fi

# Check if Project exists
if [ ! -d "$PROJECT_PATH/Assets" ]; then
    echo "Error: Project not found at: $PROJECT_PATH"
    echo "Check that the drive is mounted and the path is correct."
    exit 1
fi

# --- BUILD PROCESS ---

echo "Cleaning previous builds..."
rm -rf "$PROJECT_PATH/BuildClient"
mkdir -p "$PROJECT_PATH/BuildClient"

echo "Starting Unity Build..."

# CRITICAL FOR WSL:
# 1. Convert Project path to Windows format (D:/...)
WINDOWS_PROJECT_PATH=$(wslpath -m "$PROJECT_PATH")

# 2. Convert Log path to Windows format (D:/...)
# This ensures Unity (running in Windows) writes to the exact file we expect.
WINDOWS_LOG_PATH=$(wslpath -m "$LOG_FILE_LINUX")

echo "  Project (Linux):   $PROJECT_PATH"
echo "  Project (Windows): $WINDOWS_PROJECT_PATH"
echo "  Method:            BuildScript.BuildLinux"
echo "  Logs:              $LOG_FILE_LINUX"

# The Command
"$UNITY_PATH" \
  -quit \
  -batchmode \
  -nographics \
  -projectPath "$WINDOWS_PROJECT_PATH" \
  -executeMethod BuildScript.BuildLinux \
  -logFile "$WINDOWS_LOG_PATH"

# --- RESULT CHECKING ---

EXIT_CODE=$?

if [ $EXIT_CODE -eq 0 ]; then
    echo "-----------------------------------"
    echo "✅ Build Successful!"
    echo "Executable located at: $PROJECT_PATH/BuildClient/"
    echo "-----------------------------------"
else
    echo "-----------------------------------"
    echo "❌ Build Failed! (Exit Code: $EXIT_CODE)"
    echo "--- ERROR LOG START ---"
    # Print the log file to the screen so we can see the specific C# error
    cat "$LOG_FILE_LINUX"
    echo "--- ERROR LOG END ---"
    echo "-----------------------------------"
    exit 1
fi


read -p "Press ENTER to exit..."
