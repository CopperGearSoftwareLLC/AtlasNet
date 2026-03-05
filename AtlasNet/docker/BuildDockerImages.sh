#!/usr/bin/env bash
set -euo pipefail

# Default platforms (used only if user wants to override)
DEFAULT_PLATFORMS="linux/amd64,linux/arm64"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ATLASNET_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Default bake file location
BAKE_FILE="${BAKE_FILE:-${SCRIPT_DIR}/dockerfiles/docker-bake.json}"

# Builder name
BUILDER_NAME="${ATLASNET_DOCKER_BUILDER_NAME:-atlasnet-builder}"
BUILDKITD_CONFIG="${BUILDKITD_CONFIG:-${SCRIPT_DIR}/buildkitd.toml}"
CHANGED_ONLY="${ATLASNET_DOCKER_CHANGED_ONLY:-0}"
TARGET_OVERRIDE=""

RECREATE_BUILDER="${ATLASNET_DOCKER_RECREATE_BUILDER:-}"
STATE_FILE_OVERRIDE="${ATLASNET_DOCKER_BUILD_STATE_FILE:-}"

usage() {
    echo "Usage: $0 [-p platforms] [-f bake_file] [-c] [-t targets]"
    echo "  -p    Comma-separated platforms (default: use bake file settings)"
    echo "  -f    Path to docker-bake.json (default: $BAKE_FILE)"
    echo "  -c    Changed-only mode (cache builder + build changed targets only)"
    echo "  -t    Comma-separated explicit bake targets"
    exit 1
}

PLATFORMS=""

while getopts "p:f:t:ch" opt; do
    case "$opt" in
        p) PLATFORMS="$OPTARG" ;;
        f) BAKE_FILE="$OPTARG" ;;
        t) TARGET_OVERRIDE="$OPTARG" ;;
        c) CHANGED_ONLY=1 ;;
        h) usage ;;
        *) usage ;;
    esac
done

if [[ -z "$RECREATE_BUILDER" ]]; then
    if [[ "$CHANGED_ONLY" == "1" ]]; then
        RECREATE_BUILDER="0"
    else
        RECREATE_BUILDER="1"
    fi
fi

trim() {
    local value="$1"
    value="${value#"${value%%[![:space:]]*}"}"
    value="${value%"${value##*[![:space:]]}"}"
    printf '%s' "$value"
}

sha256_stdin() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 | awk '{print $1}'
    else
        echo "Error: sha256sum (or shasum) is required." >&2
        exit 1
    fi
}

hash_file() {
    local file="$1"
    if [[ ! -f "$file" ]]; then
        printf 'missing'
        return
    fi
    <"$file" sha256_stdin
}

hash_directory() {
    local dir="$1"
    if [[ ! -d "$dir" ]]; then
        printf 'missing'
        return
    fi

    local count=0
    local file rel digest
    {
        while IFS= read -r -d '' file; do
            count=1
            rel="${file#${dir}/}"
            digest="$(hash_file "$file")"
            printf '%s %s\n' "$digest" "$rel"
        done < <(find "$dir" -type f -print0 | sort -z)

        if ((count == 0)); then
            printf 'empty\n'
        fi
    } | sha256_stdin
}

composite_hash() {
    printf '%s\n' "$@" | sha256_stdin
}

project_state_file() {
    local key
    key="$(printf '%s' "$ATLASNET_ROOT" | sha256_stdin | cut -c1-16)"
    printf '/tmp/atlasnet-docker-build-state-%s.txt' "$key"
}

image_exists() {
    local tag="$1"
    docker image inspect "$tag" >/dev/null 2>&1
}

target_image_tag() {
    case "$1" in
        watchdog) printf 'watchdog:latest' ;;
        proxy) printf 'proxy:latest' ;;
        shard) printf 'shard:latest' ;;
        cartograph) printf 'cartograph:latest' ;;
        atlasnetsdk) printf 'atlasnetsdk:latest' ;;
        *) printf '' ;;
    esac
}

determine_changed_targets() {
    local stage_dir="$1"
    local global_hash
    declare -A prev_hashes=()
    declare -A current_hashes=()
    local target image_tag prev_hash

    if [[ -f "$STATE_FILE" ]]; then
        while IFS='|' read -r target hash; do
            [[ -z "${target:-}" || -z "${hash:-}" ]] && continue
            prev_hashes["$target"]="$hash"
        done <"$STATE_FILE"
    fi

    global_hash="$(
        composite_hash \
            "$(hash_file "$BAKE_FILE")" \
            "$(hash_directory "${ATLASNET_ROOT}/docker/dockerfiles")" \
            "$(hash_directory "${ATLASNET_ROOT}/scripts/Dockerfiles")" \
            "$(hash_file "${SCRIPT_DIR}/BuildDockerImages.sh")"
    )"

    current_hashes["watchdog"]="$(composite_hash "$global_hash" "$(hash_file "${stage_dir}/watchdog")")"
    current_hashes["proxy"]="$(composite_hash "$global_hash" "$(hash_file "${stage_dir}/proxy")")"
    current_hashes["shard"]="$(composite_hash "$global_hash" "$(hash_file "${stage_dir}/shard")")"
    current_hashes["cartograph"]="$(composite_hash "$global_hash" "$(hash_directory "${stage_dir}/cartograph")" "$(hash_file "${stage_dir}/Web.node")")"
    current_hashes["atlasnetsdk"]="$(composite_hash "$global_hash" "$(hash_directory "${stage_dir}")")"

    declare -a targets=("watchdog" "proxy" "shard" "cartograph")
    declare -a changed=()
    for target in "${targets[@]}"; do
        image_tag="$(target_image_tag "$target")"
        prev_hash="${prev_hashes[$target]:-}"
        if [[ -z "$prev_hash" || "$prev_hash" != "${current_hashes[$target]}" ]]; then
            changed+=("$target")
            continue
        fi
        if [[ -n "$image_tag" ]] && ! image_exists "$image_tag"; then
            changed+=("$target")
        fi
    done

    printf '%s\n' "${changed[@]}"
}

write_state_file_from_stage() {
    local stage_dir="$1"
    local global_hash
    declare -A current_hashes=()
    local target

    global_hash="$(
        composite_hash \
            "$(hash_file "$BAKE_FILE")" \
            "$(hash_directory "${ATLASNET_ROOT}/docker/dockerfiles")" \
            "$(hash_directory "${ATLASNET_ROOT}/scripts/Dockerfiles")" \
            "$(hash_file "${SCRIPT_DIR}/BuildDockerImages.sh")"
    )"

    current_hashes["watchdog"]="$(composite_hash "$global_hash" "$(hash_file "${stage_dir}/watchdog")")"
    current_hashes["proxy"]="$(composite_hash "$global_hash" "$(hash_file "${stage_dir}/proxy")")"
    current_hashes["shard"]="$(composite_hash "$global_hash" "$(hash_file "${stage_dir}/shard")")"
    current_hashes["cartograph"]="$(composite_hash "$global_hash" "$(hash_directory "${stage_dir}/cartograph")" "$(hash_file "${stage_dir}/Web.node")")"
    current_hashes["atlasnetsdk"]="$(composite_hash "$global_hash" "$(hash_directory "${stage_dir}")")"

    mkdir -p "$(dirname "$STATE_FILE")"
    {
        for target in "${!current_hashes[@]}"; do
            printf '%s|%s\n' "$target" "${current_hashes[$target]}"
        done | sort
    } >"$STATE_FILE"
}

# Determine progress style
if [ -t 1 ]; then
    PROGRESS="tty"
else
    PROGRESS="plain"
fi

echo "==> Using bake file: $BAKE_FILE"
[ -n "$PLATFORMS" ] && echo "==> Using platforms override: $PLATFORMS"
[ "$CHANGED_ONLY" = "1" ] && echo "==> Changed-only mode enabled"
[ -n "$TARGET_OVERRIDE" ] && echo "==> Explicit targets: $TARGET_OVERRIDE"

# Ensure buildx exists
if ! docker buildx version >/dev/null 2>&1; then
    echo "Docker buildx is required."
    exit 1
fi

create_builder() {
    echo "==> Creating buildx builder: $BUILDER_NAME"
    docker buildx create \
        --name "$BUILDER_NAME" \
        --driver docker-container \
        --driver-opt network=host \
        --buildkitd-config "$BUILDKITD_CONFIG" \
        --use
        #--buildkitd-flags '--allow-insecure-entitlement=network.host' \
}

# Create builder if it doesn't exist
if [[ "$RECREATE_BUILDER" == "1" ]]; then
    echo "==> Recreating buildx builder: $BUILDER_NAME"
    docker buildx rm -f "$BUILDER_NAME" >/dev/null 2>&1 || true
fi

if ! docker buildx inspect "$BUILDER_NAME" >/dev/null 2>&1; then
    create_builder
else
    echo "==> Reusing buildx builder: $BUILDER_NAME"
    docker buildx use "$BUILDER_NAME"
fi

docker buildx inspect --bootstrap >/dev/null

if [[ ! -f "$BAKE_FILE" ]]; then
    echo "Error: bake file not found: $BAKE_FILE" >&2
    exit 1
fi

declare -a TARGETS_TO_BUILD=()
declare -A TARGET_SEEN=()
if [[ -n "$TARGET_OVERRIDE" ]]; then
    IFS=',' read -r -a explicit_targets <<< "$TARGET_OVERRIDE"
    for raw_target in "${explicit_targets[@]}"; do
        target="$(trim "$raw_target")"
        [[ -z "$target" || -n "${TARGET_SEEN[$target]:-}" ]] && continue
        TARGET_SEEN["$target"]=1
        TARGETS_TO_BUILD+=("$target")
    done
elif [[ "$CHANGED_ONLY" == "1" ]]; then
    STAGE_DIR="${ATLASNET_ROOT}/.stage"
    if [[ ! -d "$STAGE_DIR" ]]; then
        echo "Error: staged inputs directory not found: $STAGE_DIR" >&2
        exit 1
    fi

    STATE_FILE="${STATE_FILE_OVERRIDE:-$(project_state_file)}"
    while IFS= read -r target; do
        [[ -z "$target" ]] && continue
        TARGETS_TO_BUILD+=("$target")
    done < <(determine_changed_targets "$STAGE_DIR")

    if ((${#TARGETS_TO_BUILD[@]} == 0)); then
        if [[ -z "$TARGET_OVERRIDE" ]]; then
            write_state_file_from_stage "$STAGE_DIR"
        fi
        echo "==> No image changes detected; skipping docker buildx bake."
        echo "==> Done."
        exit 0
    fi
fi

if ((${#TARGETS_TO_BUILD[@]} > 0)); then
    echo "==> Building selected targets: ${TARGETS_TO_BUILD[*]}"
else
    echo "==> Building all bake targets in parallel with BuildKit..."
fi

# Conditionally include platforms if specified
declare -a BAKE_CMD=(docker buildx bake -f "$BAKE_FILE" --progress="$PROGRESS")
if [ -n "$PLATFORMS" ]; then
    BAKE_CMD+=(--set "*.platform=$PLATFORMS")
fi
BAKE_CMD+=(--load)
if ((${#TARGETS_TO_BUILD[@]} > 0)); then
    BAKE_CMD+=("${TARGETS_TO_BUILD[@]}")
fi

"${BAKE_CMD[@]}"

if [[ "$CHANGED_ONLY" == "1" && -z "$TARGET_OVERRIDE" ]]; then
    write_state_file_from_stage "$STAGE_DIR"
fi

echo "==> Done."
