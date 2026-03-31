#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG_FILE="$ROOT_DIR/config/cluster.env"
ENV_FILE="$ROOT_DIR/.env"
KUBECONFIG_PATH="$ROOT_DIR/config/kubeconfig"

die() { echo "ERROR: $*" >&2; exit 1; }
need_cmd() { command -v "$1" >/dev/null 2>&1 || die "Missing '$1'. Install it first."; }

if [[ ! -f "$KUBECONFIG_PATH" ]]; then
  die "Missing kubeconfig: $KUBECONFIG_PATH (run ./scripts/bootstrap.sh first)"
fi

# Optional config sources:
# - .env is the primary project config
# - config/cluster.env can override values when present
if [[ -f "$ENV_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$ENV_FILE"
fi
if [[ -f "$CONFIG_FILE" ]]; then
  # shellcheck disable=SC1090
  source "$CONFIG_FILE"
fi

: "${INSTALL_METRICS_SERVER:=true}"
: "${INSTALL_METALLB:=false}"
: "${METALLB_ADDRESS_POOL:=}"
: "${INSTALL_INGRESS_NGINX:=false}"
: "${INSTALL_CERT_MANAGER:=false}"

need_cmd kubectl
need_cmd helm

export KUBECONFIG="$KUBECONFIG_PATH"

# Namespaces
kubectl create namespace platform >/dev/null 2>&1 || true

wait_for_service_load_balancer() {
  local namespace="$1"
  local service_name="$2"
  local timeout_seconds="${3:-120}"
  local elapsed=0
  local endpoint=""

  while ((elapsed < timeout_seconds)); do
    endpoint="$(kubectl -n "$namespace" get service "$service_name" -o jsonpath='{.status.loadBalancer.ingress[0].ip}{.status.loadBalancer.ingress[0].hostname}' 2>/dev/null || true)"
    if [[ -n "$endpoint" ]]; then
      printf '%s\n' "$endpoint"
      return 0
    fi

    sleep 1
    elapsed=$((elapsed + 1))
  done

  return 1
}

first_node_access_ip() {
  local node_ip=""

  node_ip="$(kubectl get nodes -o jsonpath='{.items[0].status.addresses[?(@.type=="ExternalIP")].address}' 2>/dev/null || true)"
  if [[ -z "$node_ip" ]]; then
    node_ip="$(kubectl get nodes -o jsonpath='{.items[0].status.addresses[?(@.type=="InternalIP")].address}' 2>/dev/null || true)"
  fi

  printf '%s\n' "$node_ip"
}

report_service_access() {
  local label="$1"
  local namespace="$2"
  local service_name="$3"
  local scheme="${4:-http}"
  local path="${5:-}"
  local service_type service_port endpoint node_ip node_port

  if ! kubectl -n "$namespace" get service "$service_name" >/dev/null 2>&1; then
    return 0
  fi

  service_type="$(kubectl -n "$namespace" get service "$service_name" -o jsonpath='{.spec.type}' 2>/dev/null || true)"
  service_port="$(kubectl -n "$namespace" get service "$service_name" -o jsonpath='{.spec.ports[0].port}' 2>/dev/null || true)"
  service_port="${service_port:-80}"

  case "$service_type" in
    LoadBalancer)
      endpoint="$(wait_for_service_load_balancer "$namespace" "$service_name" 60 || true)"
      if [[ -n "$endpoint" ]]; then
        echo " - ${label}: ${scheme}://${endpoint}:${service_port}${path}"
      else
        echo " - ${label}: service is LoadBalancer but external IP is still pending"
      fi
      ;;
    NodePort)
      node_ip="$(first_node_access_ip)"
      node_port="$(kubectl -n "$namespace" get service "$service_name" -o jsonpath='{.spec.ports[0].nodePort}' 2>/dev/null || true)"
      if [[ -n "$node_ip" && -n "$node_port" ]]; then
        echo " - ${label}: ${scheme}://${node_ip}:${node_port}${path}"
      else
        echo " - ${label}: service is NodePort but no node endpoint was detected"
      fi
      ;;
    *)
      echo " - ${label}: service type is ${service_type:-unknown}; use kubectl port-forward if you need host access"
      ;;
  esac
}

install_metrics_server() {
  # Lightweight metrics server for `kubectl top`.
  helm repo add metrics-server https://kubernetes-sigs.github.io/metrics-server/ >/dev/null
  helm repo update >/dev/null

  local release_exists="false"
  local sa_exists="false"

  if helm status metrics-server --namespace kube-system >/dev/null 2>&1; then
    release_exists="true"
  fi

  # k3s often ships metrics-server by default. If it already exists but is not
  # managed by this Helm release, keep the existing install and skip.
  if [[ "$release_exists" == "false" ]] && \
    kubectl get deployment metrics-server --namespace kube-system >/dev/null 2>&1; then
    echo "   metrics-server already exists in kube-system (non-Helm); skipping Helm install"
    return 0
  fi

  if kubectl get serviceaccount metrics-server --namespace kube-system >/dev/null 2>&1; then
    sa_exists="true"
  fi

  local helm_args=(
    upgrade
    --install
    metrics-server
    metrics-server/metrics-server
    --namespace
    kube-system
    --set
    args="{--kubelet-insecure-tls,--kubelet-preferred-address-types=InternalIP\,ExternalIP\,Hostname}"
    --wait
  )

  # If ServiceAccount already exists and release is new, don't ask Helm to own SA.
  if [[ "$release_exists" == "false" && "$sa_exists" == "true" ]]; then
    helm_args+=(--set serviceAccount.create=false --set serviceAccount.name=metrics-server)
  fi

  # k3s often needs insecure TLS from kubelets in homelab environments.
  helm "${helm_args[@]}"
}

install_metallb() {
  [[ -n "$METALLB_ADDRESS_POOL" ]] || die "INSTALL_METALLB=true but METALLB_ADDRESS_POOL is empty"

  helm repo add metallb https://metallb.github.io/metallb >/dev/null
  helm repo update >/dev/null

  kubectl create namespace metallb-system >/dev/null 2>&1 || true
  helm upgrade --install metallb metallb/metallb --namespace metallb-system --wait

  # Address pool (Layer2) - minimal for homelab.
  cat <<EOF | kubectl apply -f -
apiVersion: metallb.io/v1beta1
kind: IPAddressPool
metadata:
  name: default-pool
  namespace: metallb-system
spec:
  addresses:
  - ${METALLB_ADDRESS_POOL}
---
apiVersion: metallb.io/v1beta1
kind: L2Advertisement
metadata:
  name: default
  namespace: metallb-system
spec: {}
EOF
}

install_ingress_nginx() {
  helm repo add ingress-nginx https://kubernetes.github.io/ingress-nginx >/dev/null
  helm repo update >/dev/null

  kubectl create namespace ingress-nginx >/dev/null 2>&1 || true
  helm upgrade --install ingress-nginx ingress-nginx/ingress-nginx     --namespace ingress-nginx     --wait
}

install_cert_manager() {
  helm repo add jetstack https://charts.jetstack.io >/dev/null
  helm repo update >/dev/null

  kubectl create namespace cert-manager >/dev/null 2>&1 || true
  helm upgrade --install cert-manager jetstack/cert-manager     --namespace cert-manager     --set crds.enabled=true     --wait
}

echo "Installing platform add-ons (safe to re-run) ..."
if [[ "$INSTALL_METRICS_SERVER" == "true" ]]; then
  echo " - metrics-server"
  install_metrics_server
fi

if [[ "$INSTALL_METALLB" == "true" ]]; then
  echo " - MetalLB"
  install_metallb
fi

if [[ "$INSTALL_INGRESS_NGINX" == "true" ]]; then
  echo " - ingress-nginx"
  install_ingress_nginx
fi

if [[ "$INSTALL_CERT_MANAGER" == "true" ]]; then
  echo " - cert-manager"
  install_cert_manager
fi

echo
echo "Done. Current pods:"
kubectl get pods -A | head -n 50

echo
echo "Host access:"
report_service_access "Cartograph" "${ATLASNET_K8S_NAMESPACE:-atlasnet}" "atlasnet-cartograph"
