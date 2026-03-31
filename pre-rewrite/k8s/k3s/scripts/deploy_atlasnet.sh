#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="$ROOT_DIR/.env"
KUBECONFIG_PATH="$ROOT_DIR/config/kubeconfig"
CHART_DIR="$ROOT_DIR/../charts/atlasnet"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

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

[[ -f "$KUBECONFIG_PATH" ]] || die "Missing kubeconfig: $KUBECONFIG_PATH (run make k3s-deploy first)"
[[ -d "$CHART_DIR" ]] || die "Missing Helm chart dir: $CHART_DIR"

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
: "${ATLASNET_HELM_RELEASE_NAME:=atlasnet}"
: "${ATLASNET_CARTOGRAPH_INGRESS_CLASS_NAME:=nginx}"
: "${ATLASNET_CARTOGRAPH_INGRESS_HOST:=cartograph.atlasnet.local}"
: "${ATLASNET_K8S_LLM_ENABLED:=1}"
: "${ATLASNET_LLM_IMAGE:=ghcr.io/ggml-org/llama.cpp:server}"
: "${ATLASNET_LLM_SERVICE_PORT:=8080}"
: "${ATLASNET_LLM_API_FORMAT:=openai}"
: "${ATLASNET_LLM_MODEL_ID:=huggingface.co/dannys0n/qwen3-1.7b-seed_gen_voronoi:Q4_K_M}"
: "${ATLASNET_LLM_MODEL_URL:=https://huggingface.co/Dannys0n/Qwen3-1.7B-seed_gen_voronoi/resolve/main/Qwen3-1.7B-seed_gen_voronoi-Q4_K_M.gguf}"
: "${ATLASNET_LLM_MODEL_FILE_NAME:=Qwen3-1.7B-seed_gen_voronoi-Q4_K_M.gguf}"

if [[ -z "$ATLASNET_CARTOGRAPH_INGRESS_HOST" ]]; then
  die "ATLASNET_CARTOGRAPH_INGRESS_HOST must be set."
fi

if [[ -z "${DOCKERHUB_NAMESPACE}" ]] && [[ -z "${ATLASNET_WATCHDOG_IMAGE:-}" || -z "${ATLASNET_PROXY_IMAGE:-}" || -z "${ATLASNET_SANDBOX_SERVER_IMAGE:-}" || -z "${ATLASNET_CARTOGRAPH_IMAGE:-}" ]]; then
  die "Set DOCKERHUB_NAMESPACE in .env, or explicitly set all ATLASNET_*_IMAGE values."
fi

: "${ATLASNET_WATCHDOG_IMAGE:=${DOCKERHUB_NAMESPACE}/watchdog:${ATLASNET_IMAGE_TAG}}"
: "${ATLASNET_PROXY_IMAGE:=${DOCKERHUB_NAMESPACE}/proxy:${ATLASNET_IMAGE_TAG}}"
: "${ATLASNET_CARTOGRAPH_IMAGE:=${DOCKERHUB_NAMESPACE}/cartograph:${ATLASNET_IMAGE_TAG}}"
: "${ATLASNET_SANDBOX_SERVER_IMAGE:=${DOCKERHUB_NAMESPACE}/sandbox-server:${ATLASNET_IMAGE_TAG}}"

need_cmd kubectl
need_cmd helm

export KUBECONFIG="$KUBECONFIG_PATH"

if [[ -z "${SERVER_NODE_NAME:-}" ]]; then
  SERVER_NODE_NAME="$(detect_server_node_name)"
fi

echo "Deploying AtlasNet to namespace '$ATLASNET_K8S_NAMESPACE' ..."
echo " - server node: $SERVER_NODE_NAME"
echo " - watchdog image: $ATLASNET_WATCHDOG_IMAGE"
echo " - proxy image: $ATLASNET_PROXY_IMAGE"
echo " - sandbox server image: $ATLASNET_SANDBOX_SERVER_IMAGE"
echo " - cartograph image: $ATLASNET_CARTOGRAPH_IMAGE"
echo " - imagePullPolicy: $ATLASNET_IMAGE_PULL_POLICY"
echo " - cartograph ingress host: $ATLASNET_CARTOGRAPH_INGRESS_HOST"
echo " - llm enabled: $ATLASNET_K8S_LLM_ENABLED"

warn_if_mixed_arch_cluster
ensure_namespace
configure_dockerhub_pull_secret

helm upgrade --install "$ATLASNET_HELM_RELEASE_NAME" "$CHART_DIR" \
  --namespace "$ATLASNET_K8S_NAMESPACE" \
  --set-string serverNodeName="$SERVER_NODE_NAME" \
  --set-string imagePullPolicy="$ATLASNET_IMAGE_PULL_POLICY" \
  --set-string images.watchdog="$ATLASNET_WATCHDOG_IMAGE" \
  --set-string images.proxy="$ATLASNET_PROXY_IMAGE" \
  --set-string images.shard="$ATLASNET_SANDBOX_SERVER_IMAGE" \
  --set-string images.cartograph="$ATLASNET_CARTOGRAPH_IMAGE" \
  --set-string cartograph.ingress.className="$ATLASNET_CARTOGRAPH_INGRESS_CLASS_NAME" \
  --set-string cartograph.ingress.host="$ATLASNET_CARTOGRAPH_INGRESS_HOST" \
  --set llm.enabled="$ATLASNET_K8S_LLM_ENABLED" \
  --set-string llm.image="$ATLASNET_LLM_IMAGE" \
  --set-string llm.servicePort="$ATLASNET_LLM_SERVICE_PORT" \
  --set-string llm.apiFormat="$ATLASNET_LLM_API_FORMAT" \
  --set-string llm.modelId="$ATLASNET_LLM_MODEL_ID" \
  --set-string llm.modelUrl="$ATLASNET_LLM_MODEL_URL" \
  --set-string llm.modelFileName="$ATLASNET_LLM_MODEL_FILE_NAME" \
  --wait >/dev/null

# Migration cleanup for earlier revisions where workload kinds differed.
kubectl -n "$ATLASNET_K8S_NAMESPACE" delete daemonset atlasnet-watchdog --ignore-not-found >/dev/null || true
kubectl -n "$ATLASNET_K8S_NAMESPACE" delete deployment atlasnet-cartograph --ignore-not-found >/dev/null || true
kubectl -n "$ATLASNET_K8S_NAMESPACE" delete deployment atlasnet-proxy --ignore-not-found >/dev/null || true

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
echo "Cartograph ingress: http://$ATLASNET_CARTOGRAPH_INGRESS_HOST"
