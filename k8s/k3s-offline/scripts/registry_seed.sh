#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
source_env

need_cmd docker
need_cmd helm
need_cmd curl
docker info >/dev/null 2>&1 || die "Docker daemon is not reachable."
docker buildx version >/dev/null 2>&1 || die "Docker buildx is required."

bundle_root="$(bundle_dir)"
warnings_file="$bundle_root/registry-warnings.txt"
: > "$warnings_file"

[[ -f "$bundle_root/bundle-manifest.env" ]] || die "Missing bundle manifest. Run 'make offline-bundle' first."

: "${INSTALL_METRICS_SERVER:=true}"
: "${INSTALL_METALLB:=true}"
: "${INSTALL_INGRESS_NGINX:=true}"
: "${INSTALL_CERT_MANAGER:=true}"
: "${ATLASNET_K8S_LLM_ENABLED:=0}"
: "${ATLASNET_LLM_HOST_PROXY_ENABLED:=1}"
: "${ATLASNET_LLM_HOST_PROXY_IMAGE:=docker.io/alpine/socat:1.8.0.1}"
: "${ATLASNET_LLM_MODEL_URL:=https://huggingface.co/Dannys0n/Qwen3-1.7B-seed_gen_voronoi/resolve/main/Qwen3-1.7B-seed_gen_voronoi-Q4_K_M.gguf}"

record_warning() {
  printf 'WARNING: %s\n' "$1" | tee -a "$warnings_file" >&2
}

resolve_local_path() {
  local path="$1"
  if [[ "$path" == /* ]]; then
    printf '%s\n' "$path"
  else
    printf '%s\n' "${ROOT_DIR}/${path#./}"
  fi
}

ensure_llm_model_file() {
  local model_path

  model_path="$(resolve_local_path "$ATLASNET_LLM_MODEL_FILE_PATH")"
  ATLASNET_LLM_MODEL_FILE_PATH="$model_path"

  if [[ -f "$model_path" ]]; then
    return 0
  fi

  mkdir -p "$(dirname "$model_path")"
  echo "==> Downloading offline LLM model"
  curl -fL --retry 3 --retry-delay 2 "$ATLASNET_LLM_MODEL_URL" -o "$model_path"
}

push_platform_specific_ref() {
  local source_ref="$1"
  local arch="$2"
  local target_ref="$3"
  local image_id

  if ! docker pull --platform "linux/${arch}" "$source_ref" >/dev/null 2>&1; then
    return 1
  fi

  image_id="$(docker image inspect --format '{{.Id}}' "$source_ref" 2>/dev/null || true)"
  [[ -n "$image_id" ]] || image_id="$source_ref"

  docker tag "$image_id" "$target_ref"
  docker push "$target_ref" >/dev/null
}

mirror_source_ref() {
  local source_ref="$1"
  local final_ref="$2"

  docker buildx imagetools create -t "$final_ref" "$source_ref" >/dev/null
}

collect_platform_images() {
  local refs_file="$bundle_root/platform-image-sources.txt"
  : > "$refs_file"
  collect_platform_image_sources >> "$refs_file"
  sort -u "$refs_file" -o "$refs_file"
  printf '%s\n' "$refs_file"
}

build_llm_seed_image() {
  local arch="$1"
  local target_ref="$2"
  local context_dir
  local status=0
  context_dir="$(mktemp -d)"

  cp "$ATLASNET_LLM_MODEL_FILE_PATH" "$context_dir/model.gguf"
  if docker buildx build \
    --platform "linux/${arch}" \
    --load \
    -t "$target_ref" \
    -f "$ROOT_DIR/docker/llm-model-seed.Dockerfile" \
    "$context_dir" >/dev/null; then
    status=0
  else
    status=$?
  fi

  rm -rf "$context_dir"
  return "$status"
}

seed_platform_registry() {
  local refs_file ref final_ref
  refs_file="$(collect_platform_images)"
  while read -r ref; do
    [[ -n "$ref" ]] || continue
    final_ref="$(local_mirror_final_ref "$ref")"
    if ! mirror_source_ref "$ref" "$final_ref"; then
      record_warning "failed to mirror platform image: ${ref}"
    fi
  done < "$refs_file"
}

seed_atlasnet_registry() {
  local component source_ref deployed_ref final_ref arch arch_ref seeded_arches=()

  for component in watchdog proxy sandbox-server cartograph; do
    seeded_arches=()
    deployed_ref="$(atlasnet_target_ref "$component")"
    for arch in $(offline_arches); do
      source_ref="$(atlasnet_source_ref "$component" "$arch")"
      arch_ref="$(local_mirror_arch_ref "$deployed_ref" "$arch")"
      if push_platform_specific_ref "$source_ref" "$arch" "$arch_ref"; then
        seeded_arches+=("$arch_ref")
      else
        record_warning "AtlasNet source image missing for ${arch}: ${source_ref}"
      fi
    done

    final_ref="$(local_mirror_final_ref "$deployed_ref")"
    if [[ "${#seeded_arches[@]}" -eq 1 ]]; then
      docker pull "${seeded_arches[0]}" >/dev/null
      docker tag "${seeded_arches[0]}" "$final_ref"
      docker push "$final_ref" >/dev/null
    elif [[ "${#seeded_arches[@]}" -gt 1 ]]; then
      docker buildx imagetools create -t "$final_ref" "${seeded_arches[@]}" >/dev/null
    else
      record_warning "no AtlasNet architectures available for ${component}"
    fi
  done

  if ! mirror_source_ref "$ATLASNET_INTERNALDB_IMAGE" "$(local_mirror_final_ref "$ATLASNET_INTERNALDB_IMAGE")"; then
    record_warning "failed to mirror internal DB image: ${ATLASNET_INTERNALDB_IMAGE}"
  fi

  if [[ "$ATLASNET_K8S_LLM_ENABLED" == "1" || "$ATLASNET_K8S_LLM_ENABLED" == "true" ]]; then
    if ! mirror_source_ref "$ATLASNET_LLM_IMAGE" "$(local_mirror_final_ref "$ATLASNET_LLM_IMAGE")"; then
      record_warning "failed to mirror LLM runtime image: ${ATLASNET_LLM_IMAGE}"
    fi

    seeded_arches=()
    for arch in $(offline_arches); do
      arch_ref="$(local_mirror_arch_ref "$ATLASNET_LLM_MODEL_SEED_IMAGE" "$arch")"
      if build_llm_seed_image "$arch" "$arch_ref"; then
        docker push "$arch_ref" >/dev/null
        seeded_arches+=("$arch_ref")
      else
        record_warning "failed to build LLM seed image for ${arch}"
      fi
    done
    if [[ "${#seeded_arches[@]}" -eq 1 ]]; then
      docker pull "${seeded_arches[0]}" >/dev/null
      docker tag "${seeded_arches[0]}" "$(local_mirror_final_ref "$ATLASNET_LLM_MODEL_SEED_IMAGE")"
      docker push "$(local_mirror_final_ref "$ATLASNET_LLM_MODEL_SEED_IMAGE")" >/dev/null
    elif [[ "${#seeded_arches[@]}" -gt 1 ]]; then
      docker buildx imagetools create -t "$(local_mirror_final_ref "$ATLASNET_LLM_MODEL_SEED_IMAGE")" "${seeded_arches[@]}" >/dev/null
    else
      record_warning "no LLM seed images were built"
    fi
  elif [[ "$ATLASNET_LLM_HOST_PROXY_ENABLED" == "1" || "$ATLASNET_LLM_HOST_PROXY_ENABLED" == "true" ]]; then
    if ! mirror_source_ref "$ATLASNET_LLM_HOST_PROXY_IMAGE" "$(local_mirror_final_ref "$ATLASNET_LLM_HOST_PROXY_IMAGE")"; then
      record_warning "failed to mirror LLM host proxy image: ${ATLASNET_LLM_HOST_PROXY_IMAGE}"
    fi
  fi
}

echo "Seeding local registry at $(registry_local_hostport) ..."
if [[ "$ATLASNET_K8S_LLM_ENABLED" == "1" || "$ATLASNET_K8S_LLM_ENABLED" == "true" ]]; then
  ensure_llm_model_file
fi
seed_platform_registry
seed_atlasnet_registry

echo
echo "Registry seeding complete."
if [[ -s "$warnings_file" ]]; then
  echo "Warnings were recorded in: $warnings_file" >&2
fi
