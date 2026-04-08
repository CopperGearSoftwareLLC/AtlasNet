#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
source_env

need_cmd curl
need_cmd helm
need_cmd sha256sum
need_cmd mktemp

: "${K3S_VERSION:?K3S_VERSION is required in .env}"
: "${K3SUP_VERSION:?K3SUP_VERSION is required in .env}"
: "${DOCKERHUB_NAMESPACE:?DOCKERHUB_NAMESPACE is required in .env}"
: "${ATLASNET_IMAGE_TAG:?ATLASNET_IMAGE_TAG is required in .env}"
: "${ATLASNET_IMAGE_TAG_AMD64:?ATLASNET_IMAGE_TAG_AMD64 is required in .env}"
: "${ATLASNET_IMAGE_TAG_ARM64:?ATLASNET_IMAGE_TAG_ARM64 is required in .env}"
: "${ATLASNET_INTERNALDB_IMAGE:?ATLASNET_INTERNALDB_IMAGE is required in .env}"
: "${INSTALL_METRICS_SERVER:=true}"
: "${INSTALL_METALLB:=true}"
: "${INSTALL_INGRESS_NGINX:=true}"
: "${INSTALL_CERT_MANAGER:=true}"
: "${ATLASNET_K8S_LLM_ENABLED:=0}"
: "${ATLASNET_LLM_MODEL_URL:=https://huggingface.co/Dannys0n/Qwen3-1.7B-seed_gen_voronoi/resolve/main/Qwen3-1.7B-seed_gen_voronoi-Q4_K_M.gguf}"

require_pinned_tag "ATLASNET_IMAGE_TAG" "$ATLASNET_IMAGE_TAG"
require_pinned_tag "ATLASNET_IMAGE_TAG_AMD64" "$ATLASNET_IMAGE_TAG_AMD64"
require_pinned_tag "ATLASNET_IMAGE_TAG_ARM64" "$ATLASNET_IMAGE_TAG_ARM64"
require_pinned_image "ATLASNET_INTERNALDB_IMAGE" "$ATLASNET_INTERNALDB_IMAGE"

[[ "$INSTALL_METRICS_SERVER" != "true" || -n "${METRICS_SERVER_CHART_VERSION:-}" ]] || die "Set METRICS_SERVER_CHART_VERSION in .env"
[[ "$INSTALL_METALLB" != "true" || -n "${METALLB_CHART_VERSION:-}" ]] || die "Set METALLB_CHART_VERSION in .env"
[[ "$INSTALL_INGRESS_NGINX" != "true" || -n "${INGRESS_NGINX_CHART_VERSION:-}" ]] || die "Set INGRESS_NGINX_CHART_VERSION in .env"
[[ "$INSTALL_CERT_MANAGER" != "true" || -n "${CERT_MANAGER_CHART_VERSION:-}" ]] || die "Set CERT_MANAGER_CHART_VERSION in .env"

if [[ "$ATLASNET_K8S_LLM_ENABLED" == "1" || "$ATLASNET_K8S_LLM_ENABLED" == "true" ]]; then
  : "${ATLASNET_LLM_IMAGE:?ATLASNET_LLM_IMAGE is required when LLM is enabled}"
  : "${ATLASNET_LLM_MODEL_FILE_PATH:?ATLASNET_LLM_MODEL_FILE_PATH is required when LLM is enabled}"
  : "${ATLASNET_LLM_MODEL_URL:?ATLASNET_LLM_MODEL_URL is required when LLM is enabled}"
  : "${ATLASNET_LLM_MODEL_SEED_IMAGE:?ATLASNET_LLM_MODEL_SEED_IMAGE is required when LLM is enabled}"
fi

bundle_root="$(bundle_dir)"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

mkdir -p "$bundle_root/tools" "$bundle_root/k3s" "$bundle_root/charts"

resolve_local_path() {
  local path="$1"
  if [[ "$path" == /* ]]; then
    printf '%s\n' "$path"
  else
    printf '%s\n' "${ROOT_DIR}/${path#./}"
  fi
}

download_file() {
  local url="$1"
  local dest="$2"
  echo "==> Downloading ${url##*/}"
  curl -fsSL "$url" -o "$dest"
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

prepare_bundle_tools() {
  local k3sup_tmp="$tmp_dir/k3sup"
  mkdir -p "$k3sup_tmp"

  echo "==> Vendoring k3sup ${K3SUP_VERSION}"
  (
    cd "$k3sup_tmp"
    curl -fsSL https://get.k3sup.dev | sh -s -- "$K3SUP_VERSION" >/dev/null
  )
  install -m 0755 "$k3sup_tmp/k3sup" "$bundle_root/tools/k3sup"

  download_file "https://get.k3s.io" "$bundle_root/k3s/install.sh"
  chmod +x "$bundle_root/k3s/install.sh"

  local arch arch_dir binary_asset airgap_asset base_url
  base_url="https://github.com/k3s-io/k3s/releases/download/${K3S_VERSION}"
  while read -r arch; do
    arch_dir="$bundle_root/k3s/$arch"
    mkdir -p "$arch_dir"
    binary_asset="$(k3s_binary_asset "$arch")"
    airgap_asset="$(k3s_airgap_asset "$arch")"
    download_file "${base_url}/${binary_asset}" "$arch_dir/k3s"
    chmod +x "$arch_dir/k3s"
    download_file "${base_url}/${airgap_asset}" "$arch_dir/${airgap_asset}"
  done < <(offline_arches)
}

prepare_chart_archives() {
  echo "==> Vendoring Helm charts"
  if [[ "$INSTALL_METRICS_SERVER" == "true" ]]; then
    helm pull metrics-server \
      --repo https://kubernetes-sigs.github.io/metrics-server/ \
      --version "$METRICS_SERVER_CHART_VERSION" \
      --destination "$bundle_root/charts" >/dev/null
  fi
  if [[ "$INSTALL_METALLB" == "true" ]]; then
    helm pull metallb \
      --repo https://metallb.github.io/metallb \
      --version "$METALLB_CHART_VERSION" \
      --destination "$bundle_root/charts" >/dev/null
  fi
  if [[ "$INSTALL_INGRESS_NGINX" == "true" ]]; then
    helm pull ingress-nginx \
      --repo https://kubernetes.github.io/ingress-nginx \
      --version "$INGRESS_NGINX_CHART_VERSION" \
      --destination "$bundle_root/charts" >/dev/null
  fi
  if [[ "$INSTALL_CERT_MANAGER" == "true" ]]; then
    helm pull cert-manager \
      --repo https://charts.jetstack.io \
      --version "$CERT_MANAGER_CHART_VERSION" \
      --destination "$bundle_root/charts" >/dev/null
  fi
}

write_bundle_manifest() {
  local manifest="$bundle_root/bundle-manifest.env"
  cat > "$manifest" <<EOF
BUNDLE_CREATED_AT=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
K3S_VERSION=$K3S_VERSION
K3SUP_VERSION=$K3SUP_VERSION
ATLASNET_IMAGE_TAG=$ATLASNET_IMAGE_TAG
ATLASNET_IMAGE_TAG_AMD64=$ATLASNET_IMAGE_TAG_AMD64
ATLASNET_IMAGE_TAG_ARM64=$ATLASNET_IMAGE_TAG_ARM64
ATLASNET_INTERNALDB_IMAGE=$ATLASNET_INTERNALDB_IMAGE
ATLASNET_OFFLINE_ARCHES=$(offline_arches | xargs)
ATLASNET_OFFLINE_REGISTRY=$(registry_advertise_hostport)
ATLASNET_OFFLINE_REGISTRY_LOCAL=$(registry_local_hostport)
INSTALL_METRICS_SERVER=$INSTALL_METRICS_SERVER
INSTALL_METALLB=$INSTALL_METALLB
INSTALL_INGRESS_NGINX=$INSTALL_INGRESS_NGINX
INSTALL_CERT_MANAGER=$INSTALL_CERT_MANAGER
METRICS_SERVER_CHART_VERSION=${METRICS_SERVER_CHART_VERSION:-}
METALLB_CHART_VERSION=${METALLB_CHART_VERSION:-}
INGRESS_NGINX_CHART_VERSION=${INGRESS_NGINX_CHART_VERSION:-}
CERT_MANAGER_CHART_VERSION=${CERT_MANAGER_CHART_VERSION:-}
ATLASNET_K8S_LLM_ENABLED=$ATLASNET_K8S_LLM_ENABLED
ATLASNET_LLM_IMAGE=${ATLASNET_LLM_IMAGE:-}
ATLASNET_LLM_MODEL_SEED_IMAGE=${ATLASNET_LLM_MODEL_SEED_IMAGE:-}
EOF
}

write_checksums() {
  local checksum_file="$bundle_root/checksums.txt"
  (
    cd "$bundle_root"
    find . -type f ! -name checksums.txt | LC_ALL=C sort | sed 's|^\./||' | while read -r path; do
      sha256sum "$path"
    done
  ) > "$checksum_file"
}

prepare_bundle_tools
prepare_chart_archives
if [[ "$ATLASNET_K8S_LLM_ENABLED" == "1" || "$ATLASNET_K8S_LLM_ENABLED" == "true" ]]; then
  ensure_llm_model_file
fi
write_bundle_manifest
write_checksums

echo
echo "Offline bundle created at: $bundle_root"
