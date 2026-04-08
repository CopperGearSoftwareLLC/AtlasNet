#!/usr/bin/env bash

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "$ROOT_DIR/../.." && pwd)"
ENV_FILE="${ROOT_DIR}/.env"
KUBECONFIG_PATH="${ROOT_DIR}/config/kubeconfig"
CHART_DIR="${REPO_ROOT}/k8s/charts/atlasnet"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

bundle_dir() {
  local path="${ATLASNET_OFFLINE_BUNDLE_DIR:-offline-bundle}"
  if [[ "$path" == /* ]]; then
    printf '%s\n' "$path"
  else
    printf '%s\n' "${ROOT_DIR}/${path#./}"
  fi
}

remote_bundle_dir() {
  printf '%s\n' "${ATLASNET_OFFLINE_REMOTE_DIR:-/var/lib/atlasnet-offline}"
}

default_primary_server_ip() {
  local entry user host
  : "${SERVER_IPS:?Set SERVER_IPS in .env}"
  : "${SERVER_SSH_USER:=root}"
  entry="${SERVER_IPS%% *}"
  read -r user host <<<"$(parse_node_entry "$entry" "$SERVER_SSH_USER")"
  printf '%s\n' "$host"
}

registry_host() {
  printf '%s\n' "${ATLASNET_OFFLINE_REGISTRY_HOST:-$(default_primary_server_ip)}"
}

registry_port() {
  printf '%s\n' "${ATLASNET_OFFLINE_REGISTRY_PORT:-5000}"
}

registry_advertise_hostport() {
  printf '%s:%s\n' "$(registry_host)" "$(registry_port)"
}

registry_local_hostport() {
  printf '%s:%s\n' "${ATLASNET_OFFLINE_REGISTRY_LOCAL_HOST:-localhost}" "$(registry_port)"
}

registry_endpoint() {
  printf 'http://%s\n' "$(registry_advertise_hostport)"
}

registry_container_name() {
  printf '%s\n' "${ATLASNET_OFFLINE_REGISTRY_CONTAINER_NAME:-atlasnet-offline-registry}"
}

registry_data_dir() {
  local path="${ATLASNET_OFFLINE_REGISTRY_DATA_DIR:-registry-data}"
  if [[ "$path" == /* ]]; then
    printf '%s\n' "$path"
  else
    printf '%s\n' "${ROOT_DIR}/${path#./}"
  fi
}

source_env() {
  [[ -f "$ENV_FILE" ]] || die "Missing .env at $ENV_FILE"
  # shellcheck disable=SC1090
  source "$ENV_FILE"
}

normalize_arch() {
  case "$1" in
    x86_64|amd64) printf 'amd64\n' ;;
    aarch64|arm64) printf 'arm64\n' ;;
    *) die "Unsupported architecture '$1'" ;;
  esac
}

parse_node_entry() {
  local entry="$1"
  local default_user="$2"
  local user host

  entry="${entry//\"/}"
  if [[ "$entry" == *@* ]]; then
    user="${entry%@*}"
    host="${entry#*@}"
  else
    user="$default_user"
    host="$entry"
  fi

  printf '%s %s\n' "$user" "$host"
}

for_each_node() {
  local callback="$1"
  local entry parsed_user parsed_host

  : "${SERVER_IPS:?Set SERVER_IPS in .env}"
  : "${SERVER_SSH_USER:?Set SERVER_SSH_USER in .env}"
  : "${WORKER_IPS:=}"
  : "${WORKER_SSH_USER:=pi}"

  for entry in $SERVER_IPS; do
    [[ -n "$entry" ]] || continue
    read -r parsed_user parsed_host <<<"$(parse_node_entry "$entry" "$SERVER_SSH_USER")"
    "$callback" "server" "$parsed_user" "$parsed_host"
  done

  for entry in $WORKER_IPS; do
    [[ -n "$entry" ]] || continue
    read -r parsed_user parsed_host <<<"$(parse_node_entry "$entry" "$WORKER_SSH_USER")"
    "$callback" "worker" "$parsed_user" "$parsed_host"
  done
}

ssh_key_path() {
  : "${SSH_KEY:=${HOME}/.ssh/id_ed25519}"
  printf '%s\n' "${SSH_KEY/#\~/$HOME}"
}

require_ssh_key() {
  local key
  key="$(ssh_key_path)"
  [[ -f "$key" ]] || die "SSH key file does not exist: $key"
}

require_pinned_tag() {
  local label="$1"
  local value="$2"
  [[ -n "$value" ]] || die "$label must not be empty"
  [[ "$value" != "latest" ]] || die "$label must not use the floating tag 'latest'"
}

require_pinned_image() {
  local label="$1"
  local ref="$2"
  local last tag

  [[ -n "$ref" ]] || die "$label must not be empty"
  if [[ "$ref" == *@sha256:* ]]; then
    return 0
  fi

  last="${ref##*/}"
  [[ "$last" == *:* ]] || die "$label must include a non-floating tag or digest: $ref"
  tag="${last##*:}"
  [[ "$tag" != "latest" ]] || die "$label must not use the floating tag 'latest': $ref"
}

image_ref_registry() {
  local ref="$1"
  local first="${ref%%/*}"
  if [[ "$ref" != */* ]] || [[ "$first" != *.* && "$first" != *:* && "$first" != "localhost" ]]; then
    printf 'docker.io\n'
  else
    printf '%s\n' "$first"
  fi
}

image_ref_remainder() {
  local ref="$1"
  local first="${ref%%/*}"
  if [[ "$ref" != */* ]] || [[ "$first" != *.* && "$first" != *:* && "$first" != "localhost" ]]; then
    printf '%s\n' "$ref"
  else
    printf '%s\n' "${ref#*/}"
  fi
}

image_ref_repo_path() {
  local ref="$1"
  local remainder
  remainder="$(image_ref_remainder "$ref")"
  remainder="${remainder%@*}"
  local last="${remainder##*/}"
  if [[ "$last" == *:* ]]; then
    printf '%s\n' "${remainder%:*}"
  else
    printf '%s\n' "$remainder"
  fi
}

image_ref_tag_value() {
  local ref="$1"
  if [[ "$ref" == *@* ]]; then
    printf 'sha-%s\n' "$(printf '%s' "${ref#*@}" | sed 's/^sha256://; s/[^A-Za-z0-9_.-]/-/g' | cut -c1-32)"
    return 0
  fi

  local remainder last
  remainder="$(image_ref_remainder "$ref")"
  last="${remainder##*/}"
  if [[ "$last" == *:* ]]; then
    printf '%s\n' "${last##*:}"
  else
    printf 'latest\n'
  fi
}

sanitize_path_component() {
  printf '%s\n' "$1" | sed 's/[^A-Za-z0-9._-]/-/g'
}

local_mirror_repo_path() {
  local source_ref="$1"
  local registry repo
  registry="$(sanitize_path_component "$(image_ref_registry "$source_ref")")"
  repo="$(image_ref_repo_path "$source_ref")"
  printf 'mirror/%s/%s\n' "$registry" "$repo"
}

local_mirror_final_ref() {
  local source_ref="$1"
  printf '%s/%s:%s\n' "$(registry_local_hostport)" "$(local_mirror_repo_path "$source_ref")" "$(image_ref_tag_value "$source_ref")"
}

local_mirror_arch_ref() {
  local source_ref="$1"
  local arch="$2"
  printf '%s/%s:%s-%s\n' "$(registry_local_hostport)" "$(local_mirror_repo_path "$source_ref")" "$(image_ref_tag_value "$source_ref")" "$arch"
}

local_atlasnet_final_ref() {
  local component="$1"
  printf '%s/atlasnet/%s:%s\n' "$(registry_local_hostport)" "$component" "$ATLASNET_IMAGE_TAG"
}

local_atlasnet_arch_ref() {
  local component="$1"
  local arch="$2"
  printf '%s/atlasnet/%s:%s-%s\n' "$(registry_local_hostport)" "$component" "$ATLASNET_IMAGE_TAG" "$arch"
}

local_internaldb_final_ref() {
  printf '%s/atlasnet/internaldb:%s\n' "$(registry_local_hostport)" "$ATLASNET_IMAGE_TAG"
}

local_internaldb_arch_ref() {
  local arch="$1"
  printf '%s/atlasnet/internaldb:%s-%s\n' "$(registry_local_hostport)" "$ATLASNET_IMAGE_TAG" "$arch"
}

local_llm_runtime_final_ref() {
  printf '%s/atlasnet/llm-runtime:%s\n' "$(registry_local_hostport)" "$ATLASNET_IMAGE_TAG"
}

local_llm_runtime_arch_ref() {
  local arch="$1"
  printf '%s/atlasnet/llm-runtime:%s-%s\n' "$(registry_local_hostport)" "$ATLASNET_IMAGE_TAG" "$arch"
}

local_llm_seed_final_ref() {
  printf '%s/atlasnet/llm-model-seed:%s\n' "$(registry_local_hostport)" "$ATLASNET_IMAGE_TAG"
}

local_llm_seed_arch_ref() {
  local arch="$1"
  printf '%s/atlasnet/llm-model-seed:%s-%s\n' "$(registry_local_hostport)" "$ATLASNET_IMAGE_TAG" "$arch"
}

write_registries_yaml() {
  local dest="$1"
  local reg
  cat > "$dest" <<EOF
mirrors:
  "$(registry_advertise_hostport)":
    endpoint:
      - "$(registry_endpoint)"
EOF

  while read -r reg; do
    [[ -n "$reg" ]] || continue
    cat >> "$dest" <<EOF
  "$reg":
    endpoint:
      - "$(registry_endpoint)"
    rewrite:
      "^(.*)$": "mirror/$(sanitize_path_component "$reg")/\$1"
EOF
  done < <(required_source_registries)
}

atlasnet_target_ref() {
  local component="$1"
  case "$component" in
    watchdog)
      printf '%s\n' "${ATLASNET_WATCHDOG_IMAGE:-${DOCKERHUB_NAMESPACE}/watchdog:${ATLASNET_IMAGE_TAG}}"
      ;;
    proxy)
      printf '%s\n' "${ATLASNET_PROXY_IMAGE:-${DOCKERHUB_NAMESPACE}/proxy:${ATLASNET_IMAGE_TAG}}"
      ;;
    sandbox-server)
      printf '%s\n' "${ATLASNET_SANDBOX_SERVER_IMAGE:-${DOCKERHUB_NAMESPACE}/sandbox-server:${ATLASNET_IMAGE_TAG}}"
      ;;
    cartograph)
      printf '%s\n' "${ATLASNET_CARTOGRAPH_IMAGE:-${DOCKERHUB_NAMESPACE}/cartograph:${ATLASNET_IMAGE_TAG}}"
      ;;
    *)
      die "Unknown AtlasNet component '$component'"
      ;;
  esac
}

atlasnet_source_ref() {
  local component="$1"
  local arch="$2"

  case "$component:$arch" in
    watchdog:amd64)
      printf '%s\n' "${ATLASNET_WATCHDOG_IMAGE_AMD64:-${DOCKERHUB_NAMESPACE}/watchdog:${ATLASNET_IMAGE_TAG_AMD64}}"
      ;;
    watchdog:arm64)
      printf '%s\n' "${ATLASNET_WATCHDOG_IMAGE_ARM64:-${DOCKERHUB_NAMESPACE}/watchdog:${ATLASNET_IMAGE_TAG_ARM64}}"
      ;;
    proxy:amd64)
      printf '%s\n' "${ATLASNET_PROXY_IMAGE_AMD64:-${DOCKERHUB_NAMESPACE}/proxy:${ATLASNET_IMAGE_TAG_AMD64}}"
      ;;
    proxy:arm64)
      printf '%s\n' "${ATLASNET_PROXY_IMAGE_ARM64:-${DOCKERHUB_NAMESPACE}/proxy:${ATLASNET_IMAGE_TAG_ARM64}}"
      ;;
    sandbox-server:amd64)
      printf '%s\n' "${ATLASNET_SANDBOX_SERVER_IMAGE_AMD64:-${DOCKERHUB_NAMESPACE}/sandbox-server:${ATLASNET_IMAGE_TAG_AMD64}}"
      ;;
    sandbox-server:arm64)
      printf '%s\n' "${ATLASNET_SANDBOX_SERVER_IMAGE_ARM64:-${DOCKERHUB_NAMESPACE}/sandbox-server:${ATLASNET_IMAGE_TAG_ARM64}}"
      ;;
    cartograph:amd64)
      printf '%s\n' "${ATLASNET_CARTOGRAPH_IMAGE_AMD64:-${DOCKERHUB_NAMESPACE}/cartograph:${ATLASNET_IMAGE_TAG_AMD64}}"
      ;;
    cartograph:arm64)
      printf '%s\n' "${ATLASNET_CARTOGRAPH_IMAGE_ARM64:-${DOCKERHUB_NAMESPACE}/cartograph:${ATLASNET_IMAGE_TAG_ARM64}}"
      ;;
    *)
      die "Unknown AtlasNet component/arch '$component:$arch'"
      ;;
  esac
}

offline_arches() {
  local arch
  : "${ATLASNET_OFFLINE_ARCHES:=amd64 arm64}"
  for arch in $ATLASNET_OFFLINE_ARCHES; do
    normalize_arch "$arch"
  done | awk '!seen[$0]++'
}

k3s_binary_asset() {
  case "$1" in
    amd64) printf 'k3s\n' ;;
    arm64) printf 'k3s-arm64\n' ;;
    *) die "Unsupported k3s asset architecture '$1'" ;;
  esac
}

k3s_airgap_asset() {
  case "$1" in
    amd64) printf 'k3s-airgap-images-amd64.tar.zst\n' ;;
    arm64) printf 'k3s-airgap-images-arm64.tar.zst\n' ;;
    *) die "Unsupported k3s airgap architecture '$1'" ;;
  esac
}

chart_archive_path() {
  local chart_name="$1"
  local version="$2"
  local archive

  archive="$(bundle_dir)/charts/${chart_name}-${version}.tgz"
  [[ -f "$archive" ]] || die "Missing vendored chart archive: $archive"
  printf '%s\n' "$archive"
}

render_chart_images() {
  local archive="$1"
  shift

  helm template atlasnet-offline "$archive" "$@" \
    | awk '/^[[:space:]]*image:[[:space:]]*/ {print $2}' \
    | tr -d '"' \
    | tr -d "'" \
    | awk 'NF'
}

collect_platform_image_sources() {
  : "${INSTALL_METRICS_SERVER:=true}"
  : "${INSTALL_METALLB:=false}"
  : "${INSTALL_INGRESS_NGINX:=false}"
  : "${INSTALL_CERT_MANAGER:=false}"

  if [[ "$INSTALL_METRICS_SERVER" == "true" ]]; then
    render_chart_images "$(chart_archive_path "metrics-server" "$METRICS_SERVER_CHART_VERSION")" \
      --namespace kube-system
  fi
  if [[ "$INSTALL_METALLB" == "true" ]]; then
    render_chart_images "$(chart_archive_path "metallb" "$METALLB_CHART_VERSION")" \
      --namespace metallb-system
  fi
  if [[ "$INSTALL_INGRESS_NGINX" == "true" ]]; then
    render_chart_images "$(chart_archive_path "ingress-nginx" "$INGRESS_NGINX_CHART_VERSION")" \
      --namespace ingress-nginx \
      --set-string controller.image.digest= \
      --set-string controller.image.digestChroot= \
      --set-string controller.admissionWebhooks.patch.image.digest=
  fi
  if [[ "$INSTALL_CERT_MANAGER" == "true" ]]; then
    render_chart_images "$(chart_archive_path "cert-manager" "$CERT_MANAGER_CHART_VERSION")" \
      --namespace cert-manager \
      --set crds.enabled=true
  fi
}

required_source_registries() {
  local arch component ref

  {
    collect_platform_image_sources

    for component in watchdog proxy sandbox-server cartograph; do
      for arch in $(offline_arches); do
        atlasnet_source_ref "$component" "$arch"
      done
    done

    printf '%s\n' "$ATLASNET_INTERNALDB_IMAGE"
    if [[ "${ATLASNET_K8S_LLM_ENABLED:-0}" == "1" || "${ATLASNET_K8S_LLM_ENABLED:-0}" == "true" ]]; then
      printf '%s\n' "$ATLASNET_LLM_IMAGE"
      printf '%s\n' "$ATLASNET_LLM_MODEL_SEED_IMAGE"
    elif [[ "${ATLASNET_LLM_HOST_PROXY_ENABLED:-1}" == "1" || "${ATLASNET_LLM_HOST_PROXY_ENABLED:-1}" == "true" ]]; then
      printf '%s\n' "${ATLASNET_LLM_HOST_PROXY_IMAGE:-docker.io/alpine/socat:1.8.0.1}"
    fi
  } | while read -r ref; do
    [[ -n "$ref" ]] || continue
    image_ref_registry "$ref"
  done | awk '!seen[$0]++'
}
