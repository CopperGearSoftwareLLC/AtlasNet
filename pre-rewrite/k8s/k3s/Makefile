SHELL := /usr/bin/env bash

# run in this order:
# in dev container - AtlasnetDockerBuild_Fast SandboxServerDockerBuild_Fast
# atlasnet-push
# atlasnet-merge-manifests
# ssh-setup
# sudo-setup
# k3s-deploy
# platform (if want exposed proxy)
# atlasnet-deploy



ENV_FILE ?= .env
ENV_EXPORTS := SERVER_IPS SERVER_SSH_USER WORKER_IPS WORKER_SSH_USER SSH_KEY K3S_API_ENDPOINT K3SUP_USE_SUDO K3S_EXTRA_ARGS K3S_AGENT_EXTRA_ARGS SERVER_NODE_NAME ATLASNET_K8S_NAMESPACE DOCKERHUB_NAMESPACE ATLASNET_IMAGE_TAG ATLASNET_IMAGE_TAG_AMD64 ATLASNET_IMAGE_TAG_ARM64 ATLASNET_WATCHDOG_IMAGE ATLASNET_PROXY_IMAGE ATLASNET_SANDBOX_SERVER_IMAGE ATLASNET_CARTOGRAPH_IMAGE ATLASNET_IMAGE_PULL_POLICY ATLASNET_HELM_RELEASE_NAME ATLASNET_CARTOGRAPH_INGRESS_CLASS_NAME ATLASNET_CARTOGRAPH_INGRESS_HOST DOCKERHUB_REQUIRE_AUTH DOCKERHUB_USERNAME DOCKERHUB_TOKEN DOCKERHUB_EMAIL DOCKERHUB_SECRET_NAME ATLASNET_MULTIARCH_PLATFORMS ATLASNET_MULTIARCH_BUILDER_NAME LATENCY_TARGETS LATENCY_DELAY LATENCY_JITTER LATENCY_SSH_USER

-include $(ENV_FILE)
export $(ENV_EXPORTS)

.PHONY: bootstrap nodes k3s-deploy k3s-install-recovery-hook platform node-logs-start node-logs-stop ssh-setup sudo-setup port-cleanup cleanup-cluster dependency-setup atlasnet-push atlasnet-deploy atlasnet-status atlasnet-merge-manifests latency-apply latency-apply-auto-worker latency-clear-all

bootstrap:
	./scripts/bootstrap.sh

nodes:
	KUBECONFIG="$(CURDIR)/config/kubeconfig" kubectl get nodes -o wide

ssh-setup:
	./scripts/ssh_setup.sh

sudo-setup:
	./scripts/sudo_setup.sh

dependency-setup:
	./scripts/dependency_setup.sh

port-cleanup:
	./scripts/port_cleanup.sh

cleanup-cluster:
	./scripts/cleanup_cluster.sh

k3s-deploy:
	@servers="$${SERVER_IPS}"; \
	primary="$${servers%% *}"; \
	: "$${servers:?Set SERVER_IPS in .env}"; \
	: "$${SERVER_SSH_USER:?Set SERVER_SSH_USER in .env}"; \
	workers="$$(echo "$${WORKER_IPS:-}" | xargs)"; \
	./scripts/port_cleanup.sh; \
	./scripts/bootstrap.sh \
	  --server-ips "$$servers" \
	  --server-user "$$SERVER_SSH_USER" \
	  --worker-ips "$$workers" \
	  --worker-user "$${WORKER_SSH_USER:-pi}" \
	  --ssh-key "$${SSH_KEY:-$$HOME/.ssh/id_ed25519}" \
	  --use-sudo "$${K3SUP_USE_SUDO:-true}" \
	  --api-endpoint "$${K3S_API_ENDPOINT:-$$primary}"

k3s-install-recovery-hook:
	./scripts/install_k3s_agent_network_recovery.sh

platform:
	./scripts/platform.sh

node-logs-start:
	./scripts/node_logs.sh start

node-logs-stop:
	./scripts/node_logs.sh stop

latency-apply:
	: "$${LATENCY_TARGETS:?Set LATENCY_TARGETS in .env or CLI (e.g. LATENCY_TARGETS='ubuntu0@192.168.122.41 192.168.88.12')}"
	./scripts/network_latency.sh apply "$$LATENCY_TARGETS"

latency-apply-auto-worker:
	./scripts/network_latency.sh apply-auto-worker

latency-clear-all:
	./scripts/network_latency.sh clear-all

atlasnet-deploy:
	./scripts/deploy_atlasnet.sh

atlasnet-push:
	./scripts/push_atlasnet.sh

atlasnet-merge-manifests:
	./scripts/merge_manifests.sh

atlasnet-status:
	KUBECONFIG="$(CURDIR)/config/kubeconfig" kubectl get nodes -o wide
	KUBECONFIG="$(CURDIR)/config/kubeconfig" kubectl get pods,svc -n "$${ATLASNET_K8S_NAMESPACE:-atlasnet}" -o wide
