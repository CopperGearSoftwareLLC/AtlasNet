#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_ROOT="$(cd "$ROOT_DIR/../.." && pwd)"
ENV_FILE="$ROOT_DIR/.env"
KUBECONFIG_PATH="$ROOT_DIR/config/kubeconfig"
BASE_TEMPLATE="$REPO_ROOT/deploy/k8s/overlays/k3d/atlasnet-dev.yaml"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

escape_sed_replacement() {
  printf '%s' "$1" | sed -e 's/[&|]/\\&/g'
}

ensure_namespace() {
  kubectl create namespace "$ATLASNET_K8S_NAMESPACE" --dry-run=client -o yaml | kubectl apply -f - >/dev/null
}

detect_server_node_name() {
  local detected=""

  detected="$(kubectl get nodes -l node-role.kubernetes.io/control-plane -o jsonpath='{.items[0].metadata.name}' 2>/dev/null || true)"
  if [[ -z "$detected" ]]; then
    detected="$(kubectl get nodes -l node-role.kubernetes.io/master -o jsonpath='{.items[0].metadata.name}' 2>/dev/null || true)"
  fi
  if [[ -z "$detected" ]]; then
    detected="$(kubectl get nodes -o jsonpath='{.items[0].metadata.name}' 2>/dev/null || true)"
  fi

  [[ -n "$detected" ]] || die "Could not detect a Kubernetes node name. Set SERVER_NODE_NAME in .env."
  echo "$detected"
}

warn_if_mixed_arch_cluster() {
  local arches
  local count
  arches="$(kubectl get nodes -o jsonpath='{range .items[*]}{.status.nodeInfo.architecture}{"\n"}{end}' 2>/dev/null || true)"
  count="$(printf '%s\n' "$arches" | awk 'NF' | sort -u | wc -l | tr -d ' ')"
  if [[ "${count:-0}" -gt 1 ]]; then
    echo "Warning: mixed-architecture cluster detected."
    echo "         Ensure all AtlasNet images are pushed as multi-arch manifests (linux/amd64 + linux/arm64)."
  fi
}

configure_dockerhub_pull_secret() {
  if [[ -n "${DOCKERHUB_USERNAME:-}" && -n "${DOCKERHUB_TOKEN:-}" ]]; then
    echo "Configuring Docker Hub pull secret '$DOCKERHUB_SECRET_NAME' in namespace '$ATLASNET_K8S_NAMESPACE' ..."
    kubectl -n "$ATLASNET_K8S_NAMESPACE" create secret docker-registry "$DOCKERHUB_SECRET_NAME" \
      --docker-server='https://index.docker.io/v1/' \
      --docker-username="$DOCKERHUB_USERNAME" \
      --docker-password="$DOCKERHUB_TOKEN" \
      --docker-email="$DOCKERHUB_EMAIL" \
      --dry-run=client -o yaml | kubectl apply -f - >/dev/null

    kubectl -n "$ATLASNET_K8S_NAMESPACE" patch serviceaccount default --type merge \
      -p "{\"imagePullSecrets\":[{\"name\":\"$DOCKERHUB_SECRET_NAME\"}]}" >/dev/null
    return 0
  fi

  if [[ "$DOCKERHUB_REQUIRE_AUTH" == "true" ]]; then
    die "DOCKERHUB_REQUIRE_AUTH=true but DOCKERHUB_USERNAME/DOCKERHUB_TOKEN are not set."
  fi

  echo "Docker Hub credentials not provided; assuming public images."
}

report_image_pull_issues() {
  local pod_reasons
  local had_issue=0

  pod_reasons="$(kubectl -n "$ATLASNET_K8S_NAMESPACE" get pods -o jsonpath='{range .items[*]}{.metadata.name}{"|"}{range .status.containerStatuses[*]}{.state.waiting.reason}{" "}{end}{"\n"}{end}' 2>/dev/null || true)"

  while IFS='|' read -r pod reasons; do
    [[ -z "${pod:-}" ]] && continue
    if [[ "$reasons" == *ImagePullBackOff* || "$reasons" == *ErrImagePull* ]]; then
      if ((had_issue == 0)); then
        echo
        echo "Detected image pull issues:"
      fi
      had_issue=1
      echo " - $pod: ${reasons:-unknown reason}"
    fi
  done <<< "$pod_reasons"

  if ((had_issue == 1)); then
    echo
    echo "Troubleshooting:"
    echo "  1) Verify image names/tags in .env (DOCKERHUB_NAMESPACE, ATLASNET_*_IMAGE, ATLASNET_IMAGE_TAG)."
    echo "  2) If images are private, set DOCKERHUB_USERNAME + DOCKERHUB_TOKEN in .env and rerun deploy."
    echo "  3) Confirm pull secret exists: kubectl -n $ATLASNET_K8S_NAMESPACE get secret $DOCKERHUB_SECRET_NAME"
    echo "  4) Inspect one failing pod: kubectl -n $ATLASNET_K8S_NAMESPACE describe pod <pod-name>"
    return 1
  fi

  return 0
}

wait_for_shard_ready() {
  local desired ready
  echo "Waiting for watchdog-driven shard scale-up ..."
  for _attempt in {1..90}; do
    desired="$(kubectl -n "$ATLASNET_K8S_NAMESPACE" get deployment atlasnet-shard -o jsonpath='{.spec.replicas}' 2>/dev/null || echo 0)"
    ready="$(kubectl -n "$ATLASNET_K8S_NAMESPACE" get deployment atlasnet-shard -o jsonpath='{.status.readyReplicas}' 2>/dev/null || echo 0)"
    desired="${desired:-0}"
    ready="${ready:-0}"

    if [[ "$desired" =~ ^[0-9]+$ && "$ready" =~ ^[0-9]+$ && "$desired" -gt 0 && "$ready" -ge 1 ]]; then
      echo " - shard deployment desired=${desired}, ready=${ready}"
      return 0
    fi
    sleep 2
  done
  echo "Warning: shard deployment did not report a ready replica within timeout."
}

[[ -f "$KUBECONFIG_PATH" ]] || die "Missing kubeconfig: $KUBECONFIG_PATH (run make linux-pi first)"
[[ -f "$BASE_TEMPLATE" ]] || die "Missing base template: $BASE_TEMPLATE"

if [[ -f "$ENV_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$ENV_FILE"
fi

: "${ATLASNET_K8S_NAMESPACE:=atlasnet}"
: "${ATLASNET_IMAGE_TAG:=latest}"
: "${ATLASNET_IMAGE_PULL_POLICY:=IfNotPresent}"
: "${DOCKERHUB_NAMESPACE:=}"
: "${DOCKERHUB_REQUIRE_AUTH:=false}"
: "${DOCKERHUB_SECRET_NAME:=dockerhub-regcred}"
: "${DOCKERHUB_EMAIL:=atlasnet@example.invalid}"
: "${ATLASNET_WAIT_FOR_SHARD_READY:=true}"

if [[ -z "${DOCKERHUB_NAMESPACE}" ]] && [[ -z "${ATLASNET_WATCHDOG_IMAGE:-}" || -z "${ATLASNET_PROXY_IMAGE:-}" || -z "${ATLASNET_SHARD_IMAGE:-}" || -z "${ATLASNET_CARTOGRAPH_IMAGE:-}" ]]; then
  die "Set DOCKERHUB_NAMESPACE in .env, or explicitly set all ATLASNET_*_IMAGE values."
fi

: "${ATLASNET_WATCHDOG_IMAGE:=${DOCKERHUB_NAMESPACE}/watchdog:${ATLASNET_IMAGE_TAG}}"
: "${ATLASNET_PROXY_IMAGE:=${DOCKERHUB_NAMESPACE}/proxy:${ATLASNET_IMAGE_TAG}}"
: "${ATLASNET_SHARD_IMAGE:=${DOCKERHUB_NAMESPACE}/shard:${ATLASNET_IMAGE_TAG}}"
: "${ATLASNET_CARTOGRAPH_IMAGE:=${DOCKERHUB_NAMESPACE}/cartograph:${ATLASNET_IMAGE_TAG}}"

need_cmd kubectl

export KUBECONFIG="$KUBECONFIG_PATH"

if [[ -z "${SERVER_NODE_NAME:-}" ]]; then
  SERVER_NODE_NAME="$(detect_server_node_name)"
fi

echo "Deploying AtlasNet to namespace '$ATLASNET_K8S_NAMESPACE' ..."
echo " - server node: $SERVER_NODE_NAME"
echo " - watchdog image: $ATLASNET_WATCHDOG_IMAGE"
echo " - proxy image: $ATLASNET_PROXY_IMAGE"
echo " - shard image: $ATLASNET_SHARD_IMAGE"
echo " - cartograph image: $ATLASNET_CARTOGRAPH_IMAGE"
echo " - imagePullPolicy: $ATLASNET_IMAGE_PULL_POLICY"

warn_if_mixed_arch_cluster
ensure_namespace
configure_dockerhub_pull_secret

TMP_MANIFEST="$(mktemp /tmp/atlasnet-k3s-deploy-XXXX.yaml)"
trap 'rm -f "$TMP_MANIFEST"' EXIT

sed \
  -e "s|__NAMESPACE__|$(escape_sed_replacement "$ATLASNET_K8S_NAMESPACE")|g" \
  -e "s|__SHARD_IMAGE__|$(escape_sed_replacement "$ATLASNET_SHARD_IMAGE")|g" \
  -e "s|__SERVER_NODE_NAME__|$(escape_sed_replacement "$SERVER_NODE_NAME")|g" \
  -e "s|image: watchdog:latest|image: $(escape_sed_replacement "$ATLASNET_WATCHDOG_IMAGE")|g" \
  -e "s|image: proxy:latest|image: $(escape_sed_replacement "$ATLASNET_PROXY_IMAGE")|g" \
  -e "s|image: cartograph:latest|image: $(escape_sed_replacement "$ATLASNET_CARTOGRAPH_IMAGE")|g" \
  -e "s|imagePullPolicy: IfNotPresent|imagePullPolicy: $(escape_sed_replacement "$ATLASNET_IMAGE_PULL_POLICY")|g" \
  "$BASE_TEMPLATE" >"$TMP_MANIFEST"

kubectl apply -f "$TMP_MANIFEST" >/dev/null

# Migration cleanup for earlier revisions where workload kinds differed.
kubectl -n "$ATLASNET_K8S_NAMESPACE" delete daemonset atlasnet-watchdog --ignore-not-found >/dev/null || true
kubectl -n "$ATLASNET_K8S_NAMESPACE" delete deployment atlasnet-cartograph --ignore-not-found >/dev/null || true
kubectl -n "$ATLASNET_K8S_NAMESPACE" delete deployment atlasnet-proxy --ignore-not-found >/dev/null || true
kubectl -n "$ATLASNET_K8S_NAMESPACE" delete svc atlasnet-internaldb --ignore-not-found >/dev/null || true

echo "Waiting for core workloads ..."
kubectl -n "$ATLASNET_K8S_NAMESPACE" rollout status deployment/atlasnet-internaldb --timeout=180s
kubectl -n "$ATLASNET_K8S_NAMESPACE" rollout status deployment/atlasnet-watchdog --timeout=180s
kubectl -n "$ATLASNET_K8S_NAMESPACE" rollout status statefulset/atlasnet-proxy --timeout=180s
kubectl -n "$ATLASNET_K8S_NAMESPACE" rollout status daemonset/atlasnet-cartograph --timeout=180s

if [[ "$ATLASNET_WAIT_FOR_SHARD_READY" == "true" ]]; then
  wait_for_shard_ready
fi

report_image_pull_issues

echo
echo "Deployment complete."
kubectl -n "$ATLASNET_K8S_NAMESPACE" get pods,svc -o wide
