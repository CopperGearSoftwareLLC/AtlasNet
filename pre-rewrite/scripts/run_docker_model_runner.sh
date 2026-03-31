#!/usr/bin/env bash
set -euo pipefail

MODEL_REF="${1:-hf.co/Dannys0n/Qwen3-1.7B-seed_gen_voronoi:Q4_K_M}"

require_cmd() {
    local cmd="$1"
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Error: required command '$cmd' is not installed." >&2
        exit 1
    fi
}

require_cmd docker

if ! docker model version >/dev/null 2>&1; then
    echo "==> Installing Docker Model Runner plugin..."
    sudo apt install -y docker-model-plugin
fi

if ! docker model version >/dev/null 2>&1; then
    echo "Error: 'docker model' is still unavailable after installing docker-model-plugin." >&2
    exit 1
fi

echo "==> Starting Docker model: ${MODEL_REF}"
exec docker model run "${MODEL_REF}"
