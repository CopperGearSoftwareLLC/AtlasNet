#!/usr/bin/env bash
set -euo pipefail

TEMPLATE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "${TEMPLATE_DIR}/../.." && pwd)"
ENV_FILE="${TEMPLATE_DIR}/.env"
KUBECONFIG_PATH="${TEMPLATE_DIR}/config/kubeconfig"
DEPLOY_SCRIPT="${REPO_ROOT}/Dev/RunAtlasNetK3sDeploy.sh"
TAG_RECORD_FILE="${TEMPLATE_DIR}/config/image_tag"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

declare -A OVERRIDES=()
capture_override() {
  local key="$1"
  if [[ -n "${!key+x}" ]]; then
    OVERRIDES["$key"]="${!key}"
  fi
}
restore_overrides() {
  local key
  for key in "${!OVERRIDES[@]}"; do
    printf -v "$key" '%s' "${OVERRIDES[$key]}"
    export "$key"
  done
}

for key in NAMESPACE IMAGE_REPO IMAGE_TAG IMAGE_PULL_SECRET_NAME CREATE_PULL_SECRET PROXY_PUBLIC_IP PROXY_PUBLIC_PORT IMAGE_VALKEY KUBECTL_CONTEXT REGISTRY_SERVER REGISTRY_USERNAME REGISTRY_PASSWORD REGISTRY_EMAIL; do
  capture_override "$key"
done

if [[ -f "${ENV_FILE}" ]]; then
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
fi
restore_overrides

need_cmd kubectl
need_cmd bash

[[ -f "${KUBECONFIG_PATH}" ]] || die "Missing kubeconfig: ${KUBECONFIG_PATH} (run 'make linux' first)"
[[ -x "${DEPLOY_SCRIPT}" ]] || die "Missing executable script: ${DEPLOY_SCRIPT}"

# Unprefixed names are primary.
NAMESPACE="${NAMESPACE:-atlasnet}"
IMAGE_REPO="${IMAGE_REPO:-}"
IMAGE_TAG="${IMAGE_TAG:-}"
IMAGE_PULL_SECRET_NAME="${IMAGE_PULL_SECRET_NAME:-}"
CREATE_PULL_SECRET="${CREATE_PULL_SECRET:-false}"
PROXY_PUBLIC_IP="${PROXY_PUBLIC_IP:-}"
PROXY_PUBLIC_PORT="${PROXY_PUBLIC_PORT:-2555}"
IMAGE_VALKEY="${IMAGE_VALKEY:-}"
KUBECTL_CONTEXT="${KUBECTL_CONTEXT:-}"
REGISTRY_SERVER="${REGISTRY_SERVER:-}"
REGISTRY_USERNAME="${REGISTRY_USERNAME:-}"
REGISTRY_PASSWORD="${REGISTRY_PASSWORD:-}"
REGISTRY_EMAIL="${REGISTRY_EMAIL:-atlasnet@example.local}"

: "${IMAGE_REPO:?Set IMAGE_REPO in deploy/k3s_slim/.env}"

TAG_SOURCE="env"
if [[ -z "${IMAGE_TAG}" && -f "${TAG_RECORD_FILE}" ]]; then
  IMAGE_TAG="$(head -n 1 "${TAG_RECORD_FILE}" | tr -d '[:space:]')"
  if [[ -n "${IMAGE_TAG}" ]]; then
    TAG_SOURCE="recorded"
  fi
fi
if [[ -z "${IMAGE_TAG}" ]]; then
  if git -C "${REPO_ROOT}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    IMAGE_TAG="$(git -C "${REPO_ROOT}" rev-parse --short HEAD 2>/dev/null || true)"
  fi
  if [[ -n "${IMAGE_TAG}" ]]; then
    TAG_SOURCE="git-sha"
  fi
fi
if [[ -z "${IMAGE_TAG}" ]]; then
  IMAGE_TAG="latest"
  TAG_SOURCE="fallback-latest"
fi

export ATLASNET_KUBECONFIG="${KUBECONFIG_PATH}"
export ATLASNET_K8S_NAMESPACE="${NAMESPACE}"
export ATLASNET_IMAGE_REPO="${IMAGE_REPO}"
export ATLASNET_IMAGE_TAG="${IMAGE_TAG}"
export ATLASNET_IMAGE_PULL_SECRET_NAME="${IMAGE_PULL_SECRET_NAME}"

if [[ -n "${PROXY_PUBLIC_IP}" ]]; then
  export ATLASNET_PROXY_PUBLIC_IP="${PROXY_PUBLIC_IP}"
fi
if [[ -n "${PROXY_PUBLIC_PORT}" ]]; then
  export ATLASNET_PROXY_PUBLIC_PORT="${PROXY_PUBLIC_PORT}"
fi
if [[ -n "${IMAGE_VALKEY}" ]]; then
  export ATLASNET_IMAGE_VALKEY="${IMAGE_VALKEY}"
fi
if [[ -n "${KUBECTL_CONTEXT}" ]]; then
  export ATLASNET_KUBECTL_CONTEXT="${KUBECTL_CONTEXT}"
fi

if [[ "${CREATE_PULL_SECRET}" == "true" ]]; then
  [[ -n "${IMAGE_PULL_SECRET_NAME}" ]] || die "CREATE_PULL_SECRET=true requires IMAGE_PULL_SECRET_NAME"
  : "${REGISTRY_SERVER:?Set REGISTRY_SERVER in .env}"
  : "${REGISTRY_USERNAME:?Set REGISTRY_USERNAME in .env}"
  : "${REGISTRY_PASSWORD:?Set REGISTRY_PASSWORD in .env}"

  echo "Creating/updating image pull secret '${IMAGE_PULL_SECRET_NAME}' in namespace '${NAMESPACE}'..."
  kubectl --kubeconfig "${KUBECONFIG_PATH}" create namespace "${NAMESPACE}" --dry-run=client -o yaml | \
    kubectl --kubeconfig "${KUBECONFIG_PATH}" apply -f - >/dev/null

  kubectl --kubeconfig "${KUBECONFIG_PATH}" -n "${NAMESPACE}" create secret docker-registry "${IMAGE_PULL_SECRET_NAME}" \
    --docker-server="${REGISTRY_SERVER}" \
    --docker-username="${REGISTRY_USERNAME}" \
    --docker-password="${REGISTRY_PASSWORD}" \
    --docker-email="${REGISTRY_EMAIL}" \
    --dry-run=client -o yaml | \
    kubectl --kubeconfig "${KUBECONFIG_PATH}" apply -f - >/dev/null
elif [[ -n "${IMAGE_PULL_SECRET_NAME}" ]]; then
  if ! kubectl --kubeconfig "${KUBECONFIG_PATH}" -n "${NAMESPACE}" get secret "${IMAGE_PULL_SECRET_NAME}" >/dev/null 2>&1; then
    echo "Warning: pull secret '${IMAGE_PULL_SECRET_NAME}' was not found in namespace '${NAMESPACE}'." >&2
    echo "         Set CREATE_PULL_SECRET=true or create the secret manually before deploy." >&2
  fi
fi

echo "Deploying AtlasNet to k3s..."
echo "  kubeconfig: ${ATLASNET_KUBECONFIG}"
echo "  namespace:  ${NAMESPACE}"
echo "  image repo: ${IMAGE_REPO}"
echo "  image tag:  ${IMAGE_TAG} (${TAG_SOURCE})"

"${DEPLOY_SCRIPT}" "${IMAGE_REPO}" "${IMAGE_TAG}"
