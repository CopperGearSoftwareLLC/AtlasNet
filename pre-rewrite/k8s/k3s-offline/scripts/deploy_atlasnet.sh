#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
source_env

[[ -f "$KUBECONFIG_PATH" ]] || die "Missing kubeconfig: $KUBECONFIG_PATH (run make k3s-deploy first)"
[[ -d "$CHART_DIR" ]] || die "Missing Helm chart dir: $CHART_DIR"

need_cmd kubectl
need_cmd helm

: "${ATLASNET_K8S_NAMESPACE:=atlasnet}"
: "${ATLASNET_HELM_RELEASE_NAME:=atlasnet}"
: "${ATLASNET_IMAGE_PULL_POLICY:=IfNotPresent}"
: "${ATLASNET_CARTOGRAPH_INGRESS_CLASS_NAME:=nginx}"
: "${ATLASNET_CARTOGRAPH_INGRESS_HOST:=cartograph.atlasnet.local}"
: "${ATLASNET_K8S_LLM_ENABLED:=0}"
: "${ATLASNET_LLM_ENDPOINT:=}"
: "${ATLASNET_LLM_DOCKER_HOST:=}"
: "${ATLASNET_LLM_DOCKER_PORT:=12434}"
: "${ATLASNET_LLM_HOST_PROXY_ENABLED:=1}"
: "${ATLASNET_LLM_HOST_PROXY_IMAGE:=docker.io/alpine/socat:1.8.0.1}"
: "${ATLASNET_LLM_HOST_PROXY_NAME:=atlasnet-llm-host-proxy}"
: "${ATLASNET_LLM_HOST_PROXY_SERVICE_PORT:=12434}"
: "${ATLASNET_LLM_HOST_PROXY_TARGET_PORT:=12435}"
: "${ATLASNET_LLM_SERVICE_PORT:=8080}"
: "${ATLASNET_LLM_API_FORMAT:=openai}"
: "${ATLASNET_LLM_MODEL_ID:=huggingface.co/dannys0n/qwen3-1.7b-seed_gen_voronoi:Q4_K_M}"
: "${ATLASNET_LLM_MODEL_FILE_NAME:=model.gguf}"
: "${ATLASNET_LLM_CONTEXT_SIZE:=4096}"
: "${ATLASNET_LLM_THREADS:=4}"
: "${ATLASNET_INTERNALDB_IMAGE:?ATLASNET_INTERNALDB_IMAGE is required in .env}"

: "${DOCKERHUB_NAMESPACE:=}"
[[ -n "$DOCKERHUB_NAMESPACE" || -n "${ATLASNET_WATCHDOG_IMAGE:-}" ]] || die "Set DOCKERHUB_NAMESPACE or explicit ATLASNET_*_IMAGE overrides."

: "${ATLASNET_WATCHDOG_IMAGE:=$(atlasnet_target_ref watchdog)}"
: "${ATLASNET_PROXY_IMAGE:=$(atlasnet_target_ref proxy)}"
: "${ATLASNET_SANDBOX_SERVER_IMAGE:=$(atlasnet_target_ref sandbox-server)}"
: "${ATLASNET_CARTOGRAPH_IMAGE:=$(atlasnet_target_ref cartograph)}"

if [[ "$ATLASNET_K8S_LLM_ENABLED" == "1" || "$ATLASNET_K8S_LLM_ENABLED" == "true" ]]; then
  : "${ATLASNET_LLM_IMAGE:?ATLASNET_LLM_IMAGE is required when LLM is enabled}"
  : "${ATLASNET_LLM_MODEL_SEED_IMAGE:?ATLASNET_LLM_MODEL_SEED_IMAGE is required when LLM is enabled}"
elif [[ -z "$ATLASNET_LLM_ENDPOINT" ]]; then
  if [[ "$ATLASNET_LLM_HOST_PROXY_ENABLED" == "1" || "$ATLASNET_LLM_HOST_PROXY_ENABLED" == "true" ]]; then
    ATLASNET_LLM_ENDPOINT="http://${ATLASNET_LLM_HOST_PROXY_NAME}:${ATLASNET_LLM_HOST_PROXY_SERVICE_PORT}/v1/chat/completions"
  else
    if [[ -z "$ATLASNET_LLM_DOCKER_HOST" ]]; then
      ATLASNET_LLM_DOCKER_HOST="$(default_primary_server_ip)"
    fi
    ATLASNET_LLM_ENDPOINT="http://${ATLASNET_LLM_DOCKER_HOST}:${ATLASNET_LLM_DOCKER_PORT}/v1/chat/completions"
  fi
fi

export KUBECONFIG="$KUBECONFIG_PATH"

ensure_namespace() {
  kubectl create namespace "$ATLASNET_K8S_NAMESPACE" --dry-run=client -o yaml | kubectl apply -f - >/dev/null
}

ensure_llm_host_proxy() {
  [[ "$ATLASNET_K8S_LLM_ENABLED" != "1" && "$ATLASNET_K8S_LLM_ENABLED" != "true" ]] || return 0
  [[ "$ATLASNET_LLM_HOST_PROXY_ENABLED" == "1" || "$ATLASNET_LLM_HOST_PROXY_ENABLED" == "true" ]] || return 0

  cat <<EOF | kubectl apply -f - >/dev/null
apiVersion: v1
kind: Service
metadata:
  name: ${ATLASNET_LLM_HOST_PROXY_NAME}
  namespace: ${ATLASNET_K8S_NAMESPACE}
spec:
  selector:
    app: ${ATLASNET_LLM_HOST_PROXY_NAME}
  ports:
    - name: http
      port: ${ATLASNET_LLM_HOST_PROXY_SERVICE_PORT}
      targetPort: proxy
---
apiVersion: apps/v1
kind: Deployment
metadata:
  name: ${ATLASNET_LLM_HOST_PROXY_NAME}
  namespace: ${ATLASNET_K8S_NAMESPACE}
  labels:
    app: ${ATLASNET_LLM_HOST_PROXY_NAME}
spec:
  replicas: 1
  selector:
    matchLabels:
      app: ${ATLASNET_LLM_HOST_PROXY_NAME}
  template:
    metadata:
      labels:
        app: ${ATLASNET_LLM_HOST_PROXY_NAME}
    spec:
      hostNetwork: true
      dnsPolicy: ClusterFirstWithHostNet
      affinity:
        nodeAffinity:
          requiredDuringSchedulingIgnoredDuringExecution:
            nodeSelectorTerms:
              - matchExpressions:
                  - key: node-role.kubernetes.io/control-plane
                    operator: Exists
              - matchExpressions:
                  - key: node-role.kubernetes.io/master
                    operator: Exists
              - matchExpressions:
                  - key: kubernetes.io/hostname
                    operator: In
                    values:
                      - ${SERVER_NODE_NAME}
      tolerations:
        - key: node-role.kubernetes.io/control-plane
          operator: Exists
          effect: NoSchedule
        - key: node-role.kubernetes.io/master
          operator: Exists
          effect: NoSchedule
      containers:
        - name: socat
          image: ${ATLASNET_LLM_HOST_PROXY_IMAGE}
          imagePullPolicy: ${ATLASNET_IMAGE_PULL_POLICY}
          command:
            - sh
            - -c
            - exec socat TCP-LISTEN:${ATLASNET_LLM_HOST_PROXY_TARGET_PORT},fork,reuseaddr,bind=0.0.0.0 TCP:127.0.0.1:${ATLASNET_LLM_DOCKER_PORT}
          ports:
            - name: proxy
              containerPort: ${ATLASNET_LLM_HOST_PROXY_TARGET_PORT}
              hostPort: ${ATLASNET_LLM_HOST_PROXY_TARGET_PORT}
              protocol: TCP
EOF
}

remove_llm_host_proxy() {
  kubectl -n "$ATLASNET_K8S_NAMESPACE" delete deployment "$ATLASNET_LLM_HOST_PROXY_NAME" --ignore-not-found >/dev/null || true
  kubectl -n "$ATLASNET_K8S_NAMESPACE" delete service "$ATLASNET_LLM_HOST_PROXY_NAME" --ignore-not-found >/dev/null || true
}

detect_server_node_name() {
  local detected
  detected="$(kubectl get nodes -l node-role.kubernetes.io/control-plane -o jsonpath='{.items[0].metadata.name}' 2>/dev/null || true)"
  [[ -n "$detected" ]] || detected="$(kubectl get nodes -l node-role.kubernetes.io/master -o jsonpath='{.items[0].metadata.name}' 2>/dev/null || true)"
  [[ -n "$detected" ]] || detected="$(kubectl get nodes -o jsonpath='{.items[0].metadata.name}' 2>/dev/null || true)"
  [[ -n "$detected" ]] || die "Could not detect a Kubernetes node name. Set SERVER_NODE_NAME in .env."
  printf '%s\n' "$detected"
}

report_image_pull_issues() {
  local pod_reasons had_issue=0

  pod_reasons="$(kubectl -n "$ATLASNET_K8S_NAMESPACE" get pods -o jsonpath='{range .items[*]}{.metadata.name}{"|"}{range .status.containerStatuses[*]}{.state.waiting.reason}{" "}{end}{"\n"}{end}' 2>/dev/null || true)"
  while IFS='|' read -r pod reasons; do
    [[ -n "${pod:-}" ]] || continue
    if [[ "$reasons" == *ImagePullBackOff* || "$reasons" == *ErrImagePull* ]]; then
      ((had_issue = 1))
      echo " - $pod: ${reasons:-unknown reason}"
    fi
  done <<< "$pod_reasons"

  [[ "$had_issue" -eq 0 ]]
}

wait_for_service_load_balancer() {
  local namespace="$1"
  local service_name="$2"
  local timeout_seconds="${3:-120}"
  local elapsed=0
  local endpoint=""

  while ((elapsed < timeout_seconds)); do
    endpoint="$(kubectl -n "$namespace" get service "$service_name" -o jsonpath='{.status.loadBalancer.ingress[0].ip}{.status.loadBalancer.ingress[0].hostname}' 2>/dev/null || true)"
    [[ -n "$endpoint" ]] && {
      printf '%s\n' "$endpoint"
      return 0
    }
    sleep 1
    elapsed=$((elapsed + 1))
  done

  return 1
}

report_service_access() {
  local label="$1"
  local namespace="$2"
  local service_name="$3"
  local scheme="${4:-http}"
  local path="${5:-}"
  local service_type service_port endpoint

  kubectl -n "$namespace" get service "$service_name" >/dev/null 2>&1 || return 0
  service_type="$(kubectl -n "$namespace" get service "$service_name" -o jsonpath='{.spec.type}' 2>/dev/null || true)"
  service_port="$(kubectl -n "$namespace" get service "$service_name" -o jsonpath='{.spec.ports[0].port}' 2>/dev/null || true)"
  service_port="${service_port:-80}"

  if [[ "$service_type" == "LoadBalancer" ]]; then
    endpoint="$(wait_for_service_load_balancer "$namespace" "$service_name" 60 || true)"
    if [[ -n "$endpoint" ]]; then
      echo " - ${label}: ${scheme}://${endpoint}:${service_port}${path}"
    else
      echo " - ${label}: service is LoadBalancer but external IP is still pending"
    fi
  else
    echo " - ${label}: service type is ${service_type:-unknown}"
  fi
}

report_cartograph_ingress_access() {
  local host="$ATLASNET_CARTOGRAPH_INGRESS_HOST"
  local ingress_lb=""

  ingress_lb="$(kubectl -n ingress-nginx get service ingress-nginx-controller -o jsonpath='{.status.loadBalancer.ingress[0].ip}{.status.loadBalancer.ingress[0].hostname}' 2>/dev/null || true)"

  echo
  if [[ -n "$ingress_lb" ]]; then
    echo "Cartograph ingress:"
    echo " - URL: http://${host}"
    echo " - offline LAN DNS hint: add '${ingress_lb} ${host}' to /etc/hosts on client machines"
  else
    echo "Cartograph ingress host is set to http://${host}"
    echo " - DNS for this hostname is external to the cluster; on an isolated LAN you will usually need an /etc/hosts entry once ingress-nginx gets a LoadBalancer IP."
  fi
}

if [[ -z "${SERVER_NODE_NAME:-}" ]]; then
  SERVER_NODE_NAME="$(detect_server_node_name)"
fi

ensure_namespace
if [[ "$ATLASNET_K8S_LLM_ENABLED" == "1" || "$ATLASNET_K8S_LLM_ENABLED" == "true" ]]; then
  remove_llm_host_proxy
else
  ensure_llm_host_proxy
fi

helm_args=(
  upgrade --install "$ATLASNET_HELM_RELEASE_NAME" "$CHART_DIR"
  --namespace "$ATLASNET_K8S_NAMESPACE"
  --set-string serverNodeName="$SERVER_NODE_NAME"
  --set-string imagePullPolicy="$ATLASNET_IMAGE_PULL_POLICY"
  --set-string images.watchdog="$ATLASNET_WATCHDOG_IMAGE"
  --set-string images.proxy="$ATLASNET_PROXY_IMAGE"
  --set-string images.shard="$ATLASNET_SANDBOX_SERVER_IMAGE"
  --set-string images.cartograph="$ATLASNET_CARTOGRAPH_IMAGE"
  --set-string images.internaldb="$ATLASNET_INTERNALDB_IMAGE"
  --set-string cartograph.mode="production"
  --set-string cartograph.ingress.className="$ATLASNET_CARTOGRAPH_INGRESS_CLASS_NAME"
  --set-string cartograph.ingress.host="$ATLASNET_CARTOGRAPH_INGRESS_HOST"
  --set llm.enabled="$ATLASNET_K8S_LLM_ENABLED"
  --set-string llm.image="${ATLASNET_LLM_IMAGE:-}"
  --set-string llm.endpoint="${ATLASNET_LLM_ENDPOINT:-}"
  --set-string llm.servicePort="$ATLASNET_LLM_SERVICE_PORT"
  --set-string llm.apiFormat="$ATLASNET_LLM_API_FORMAT"
  --set-string llm.modelId="$ATLASNET_LLM_MODEL_ID"
  --set-string llm.modelFileName="$ATLASNET_LLM_MODEL_FILE_NAME"
  --set-string llm.contextSize="$ATLASNET_LLM_CONTEXT_SIZE"
  --set-string llm.threads="$ATLASNET_LLM_THREADS"
  --wait
)

if [[ "$ATLASNET_K8S_LLM_ENABLED" == "1" || "$ATLASNET_K8S_LLM_ENABLED" == "true" ]]; then
  helm_args+=(
    --set llm.offline.enabled=true
    --set-string llm.offline.modelSeedImage="$ATLASNET_LLM_MODEL_SEED_IMAGE"
  )
fi

helm "${helm_args[@]}" >/dev/null

kubectl -n "$ATLASNET_K8S_NAMESPACE" rollout status deployment/atlasnet-internaldb --timeout=180s
kubectl -n "$ATLASNET_K8S_NAMESPACE" rollout status deployment/atlasnet-watchdog --timeout=180s
kubectl -n "$ATLASNET_K8S_NAMESPACE" rollout status statefulset/atlasnet-proxy --timeout=180s
kubectl -n "$ATLASNET_K8S_NAMESPACE" rollout status daemonset/atlasnet-cartograph --timeout=180s
if [[ "$ATLASNET_K8S_LLM_ENABLED" == "1" || "$ATLASNET_K8S_LLM_ENABLED" == "true" ]]; then
  kubectl -n "$ATLASNET_K8S_NAMESPACE" rollout status deployment/atlasnet-llm --timeout=300s
elif [[ "$ATLASNET_LLM_HOST_PROXY_ENABLED" == "1" || "$ATLASNET_LLM_HOST_PROXY_ENABLED" == "true" ]]; then
  kubectl -n "$ATLASNET_K8S_NAMESPACE" rollout status deployment/"$ATLASNET_LLM_HOST_PROXY_NAME" --timeout=180s
fi

echo
if ! report_image_pull_issues; then
  die "Detected image pull issues after AtlasNet deployment"
fi

kubectl -n "$ATLASNET_K8S_NAMESPACE" get pods,svc,ingress -o wide
echo
echo "Host access:"
report_service_access "AtlasNet proxy" "$ATLASNET_K8S_NAMESPACE" "atlasnet-proxy" "udp"
report_cartograph_ingress_access
if [[ -n "${ATLASNET_LLM_ENDPOINT:-}" ]]; then
  echo "LLM endpoint:"
  echo " - watchdog -> ${ATLASNET_LLM_ENDPOINT}"
  echo " - start it on the host/control machine with: make llm-runner"
fi
