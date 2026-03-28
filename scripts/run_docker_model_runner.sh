#!/usr/bin/env bash
set -euo pipefail

MODEL_REF="${1:-huggingface.co/Dannys0n/Qwen3-1.7B-seed_gen_voronoi:Q4_K_M}"
RUNNER_IMAGE="${ATLASNET_LLM_RUNNER_IMAGE:-docker/model-runner:latest-cuda}"
RUNNER_NAME="${ATLASNET_LLM_RUNNER_NAME:-docker-model-runner}"
RUNNER_PORT="${ATLASNET_LLM_DOCKER_PORT:-12434}"
RUNNER_VOLUME="${ATLASNET_LLM_RUNNER_VOLUME:-docker-model-runner-models}"
RUNNER_BIND_HOST="${ATLASNET_LLM_DOCKER_BIND_HOST:-127.0.0.1}"

require_cmd() {
    local cmd="$1"
    if ! command -v "$cmd" >/dev/null 2>&1; then
        echo "Error: required command '$cmd' is not installed." >&2
        exit 1
    fi
}

require_cmd docker

docker_image_present() {
    docker image inspect "$1" >/dev/null 2>&1
}

runner_container_running() {
    [[ "$(docker inspect -f '{{.State.Running}}' "$RUNNER_NAME" 2>/dev/null || true)" == "true" ]]
}

runner_container_exists() {
    docker inspect "$RUNNER_NAME" >/dev/null 2>&1
}

ensure_runner_container() {
    local group_args=()
    local device_args=()

    if runner_container_running; then
        return 0
    fi

    if runner_container_exists; then
        docker start "$RUNNER_NAME" >/dev/null
        return 0
    fi

    if ! docker_image_present "$RUNNER_IMAGE"; then
        return 1
    fi

    if [[ -e /dev/dri ]]; then
        device_args+=(--device /dev/dri)
        if command -v stat >/dev/null 2>&1; then
            group_args+=(--group-add "$(stat -c '%g' /dev/dri)")
        fi
    fi

    docker run -d \
        --name "$RUNNER_NAME" \
        --restart always \
        --gpus all \
        -p "${RUNNER_BIND_HOST}:${RUNNER_PORT}:12434" \
        -v "${RUNNER_VOLUME}:/models" \
        "${group_args[@]}" \
        "${device_args[@]}" \
        "$RUNNER_IMAGE" >/dev/null
}

canonicalize_model_ref() {
    local ref="$1"
    case "$ref" in
        hf.co/*) printf 'huggingface.co/%s\n' "${ref#hf.co/}" ;;
        HuggingFace.co/*) printf 'huggingface.co/%s\n' "${ref#HuggingFace.co/}" ;;
        *)
            printf '%s\n' "$ref" | tr '[:upper:]' '[:lower:]'
            ;;
    esac
}

resolve_cached_model_ref() {
    local requested_ref="$1"
    local requested_normalized cached_name cached_normalized

    requested_normalized="$(canonicalize_model_ref "$requested_ref")"

    while read -r cached_name; do
        [[ -n "$cached_name" ]] || continue
        cached_normalized="$(canonicalize_model_ref "$cached_name")"
        if [[ "$cached_normalized" == "$requested_normalized" ]]; then
            printf '%s\n' "$cached_name"
            return 0
        fi
    done < <(docker model list 2>/dev/null | awk 'NR>1 {print $1}')

    return 1
}

if ! docker model version >/dev/null 2>&1; then
    echo "==> Installing Docker Model Runner plugin..."
    sudo apt install -y docker-model-plugin
fi

if ! docker model version >/dev/null 2>&1; then
    echo "Error: 'docker model' is still unavailable after installing docker-model-plugin." >&2
    exit 1
fi

if ensure_runner_container; then
    echo "==> Using cached Docker Model Runner image: ${RUNNER_IMAGE}"
elif ! docker model status >/dev/null 2>&1; then
    echo "Error: no local Docker Model Runner container is available and Docker could not start the standalone runner." >&2
    exit 1
fi

if CACHED_MODEL_REF="$(resolve_cached_model_ref "$MODEL_REF")"; then
    echo "==> Starting cached Docker model: ${CACHED_MODEL_REF}"
    exec docker model run "${CACHED_MODEL_REF}"
fi

echo "==> Starting Docker model: ${MODEL_REF}"
exec docker model run "${MODEL_REF}"
