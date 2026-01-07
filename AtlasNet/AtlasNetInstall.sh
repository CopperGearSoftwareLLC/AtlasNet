#!/usr/bin/env bash
# Exit immediately if any command fails, catch unset variables, and fail on pipeline errors
set -euo pipefail

# get the directory this script is in
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# remove the package folder relative to the script
rm -rf "$SCRIPT_DIR/package"

# run cmake commands relative to the script directory
cmake -B "$SCRIPT_DIR/build" -S "$SCRIPT_DIR"
cmake --build "$SCRIPT_DIR/build" --parallel --target AtlasNet AtlasNetServer_Static AtlasNetServer_Shared
cmake --install "$SCRIPT_DIR/build" --component AtlasNetBootstrap