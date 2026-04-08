#!/usr/bin/env bash
set -euo pipefail

source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"
source_env

[[ -f "$KUBECONFIG_PATH" ]] || die "Missing kubeconfig: $KUBECONFIG_PATH (run make k3s-deploy first)"

need_cmd kubectl
need_cmd helm

: "${INSTALL_METRICS_SERVER:=true}"
: "${INSTALL_METALLB:=false}"
: "${INSTALL_INGRESS_NGINX:=false}"
: "${INSTALL_CERT_MANAGER:=false}"
: "${METALLB_ADDRESS_POOL:=}"

export KUBECONFIG="$KUBECONFIG_PATH"

check_conflicting_packaged_components() {
  local managed_by release_name

  if [[ "$INSTALL_METRICS_SERVER" == "true" ]] && kubectl -n kube-system get serviceaccount metrics-server >/dev/null 2>&1; then
    managed_by="$(kubectl -n kube-system get serviceaccount metrics-server -o jsonpath='{.metadata.labels.app\.kubernetes\.io/managed-by}' 2>/dev/null || true)"
    release_name="$(kubectl -n kube-system get serviceaccount metrics-server -o jsonpath='{.metadata.annotations.meta\.helm\.sh/release-name}' 2>/dev/null || true)"
    if [[ "$managed_by" != "Helm" || "$release_name" != "metrics-server" ]]; then
      die "Detected the packaged K3s metrics-server on this cluster. Recreate the cluster with 'make cleanup-cluster && make k3s-deploy' so K3s starts with '--disable metrics-server'."
    fi
  fi
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

  kubectl -n "$namespace" get service "$service_name" >/dev/null 2>&1 || return 0
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
  local archive
  archive="$(chart_archive_path "metrics-server" "$METRICS_SERVER_CHART_VERSION")"
  helm upgrade --install metrics-server "$archive" \
    --namespace kube-system \
    --set args="{--kubelet-insecure-tls,--kubelet-preferred-address-types=InternalIP\,ExternalIP\,Hostname}" \
    --wait
}

install_metallb() {
  local archive
  [[ -n "$METALLB_ADDRESS_POOL" ]] || die "INSTALL_METALLB=true but METALLB_ADDRESS_POOL is empty"
  archive="$(chart_archive_path "metallb" "$METALLB_CHART_VERSION")"
  kubectl create namespace metallb-system >/dev/null 2>&1 || true
  helm upgrade --install metallb "$archive" --namespace metallb-system --wait

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
  local archive
  archive="$(chart_archive_path "ingress-nginx" "$INGRESS_NGINX_CHART_VERSION")"
  kubectl create namespace ingress-nginx >/dev/null 2>&1 || true
  helm upgrade --install ingress-nginx "$archive" \
    --namespace ingress-nginx \
    --set-string controller.image.digest= \
    --set-string controller.image.digestChroot= \
    --set-string controller.admissionWebhooks.patch.image.digest= \
    --wait
}

install_cert_manager() {
  local archive
  archive="$(chart_archive_path "cert-manager" "$CERT_MANAGER_CHART_VERSION")"
  kubectl create namespace cert-manager >/dev/null 2>&1 || true
  helm upgrade --install cert-manager "$archive" \
    --namespace cert-manager \
    --set crds.enabled=true \
    --wait
}

if [[ "$INSTALL_METRICS_SERVER" == "true" ]]; then
  check_conflicting_packaged_components
  install_metrics_server
fi
if [[ "$INSTALL_METALLB" == "true" ]]; then
  install_metallb
fi
if [[ "$INSTALL_INGRESS_NGINX" == "true" ]]; then
  install_ingress_nginx
fi
if [[ "$INSTALL_CERT_MANAGER" == "true" ]]; then
  install_cert_manager
fi

echo
kubectl get pods -A | head -n 50
echo
echo "Host access:"
report_service_access "ingress-nginx" "ingress-nginx" "ingress-nginx-controller"
report_service_access "AtlasNet proxy" "${ATLASNET_K8S_NAMESPACE:-atlasnet}" "atlasnet-proxy" "udp"
