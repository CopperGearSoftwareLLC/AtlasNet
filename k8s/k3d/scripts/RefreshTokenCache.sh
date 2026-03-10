#!/usr/bin/env bash
set -euo pipefail

CLUSTER_NAME="${1:-atlasnet-dev}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
K3D_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
HOST_KUBECONFIG="${ATLASNET_HOST_KUBECONFIG:-${K3D_DIR}/.kube/k3d-${CLUSTER_NAME}-host.yaml}"
TEMP_KUBECONFIG="$(mktemp)"

cleanup() {
  rm -f "$TEMP_KUBECONFIG"
}
trap cleanup EXIT

k3d kubeconfig get "$CLUSTER_NAME" >"$TEMP_KUBECONFIG"

CONTEXT_NAME="$(kubectl --kubeconfig "$TEMP_KUBECONFIG" config current-context)"
CLUSTER_ID="$(kubectl --kubeconfig "$TEMP_KUBECONFIG" config view --minify -o jsonpath='{.contexts[0].context.cluster}')"
USER_ID="$(kubectl --kubeconfig "$TEMP_KUBECONFIG" config view --minify -o jsonpath='{.contexts[0].context.user}')"
API_PORT="$(kubectl --kubeconfig "$TEMP_KUBECONFIG" config view --minify -o jsonpath='{.clusters[0].cluster.server}' | sed -E 's#https://(0\\.0\\.0\\.0|127\\.0\\.0\\.1):##')"

mkdir -p "$HOME/.kube" "$(dirname "$HOST_KUBECONFIG")"
touch "$HOME/.kube/config"

KUBECONFIG="$HOME/.kube/config" kubectl config delete-context "$CONTEXT_NAME" >/dev/null 2>&1 || true
KUBECONFIG="$HOME/.kube/config" kubectl config delete-cluster "$CLUSTER_ID" >/dev/null 2>&1 || true
KUBECONFIG="$HOME/.kube/config" kubectl config unset "users.${USER_ID}" >/dev/null 2>&1 || true
KUBECONFIG="$HOME/.kube/config" k3d kubeconfig merge "$CLUSTER_NAME" --kubeconfig-merge-default --kubeconfig-switch-context --update >/dev/null
KUBECONFIG="$HOME/.kube/config" kubectl config set-cluster "$CLUSTER_ID" --server "https://127.0.0.1:${API_PORT}" >/dev/null

cp "$TEMP_KUBECONFIG" "$HOST_KUBECONFIG"
kubectl --kubeconfig "$HOST_KUBECONFIG" config set-cluster "$CLUSTER_ID" --server "https://127.0.0.1:${API_PORT}" >/dev/null

pkill -f "/opt/Headlamp/resources/headlamp-server" >/dev/null 2>&1 || true
kubectl get nodes
