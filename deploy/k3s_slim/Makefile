SHELL := /usr/bin/env bash

# run in this order:
# in dev container - AtlasnetDockerBuild_Fast
# atlasnet-push
# atlasnet-merge-manifests
# ssh-setup
# sudo-setup
# linux-pi
# atlasnet-deploy




ENV_FILE ?= .env
ENV_EXPORTS := SERVER_IP SERVER_SSH_USER WORKER_IPS WORKER_SSH_USER SSH_KEY K3S_API_ENDPOINT K3SUP_USE_SUDO SERVER_NODE_NAME ATLASNET_K8S_NAMESPACE DOCKERHUB_NAMESPACE ATLASNET_IMAGE_TAG ATLASNET_IMAGE_TAG_AMD64 ATLASNET_IMAGE_TAG_ARM64 ATLASNET_WATCHDOG_IMAGE ATLASNET_PROXY_IMAGE ATLASNET_SHARD_IMAGE ATLASNET_CARTOGRAPH_IMAGE ATLASNET_IMAGE_PULL_POLICY DOCKERHUB_REQUIRE_AUTH DOCKERHUB_USERNAME DOCKERHUB_TOKEN DOCKERHUB_EMAIL DOCKERHUB_SECRET_NAME ATLASNET_MULTIARCH_PLATFORMS ATLASNET_MULTIARCH_BUILDER_NAME

-include $(ENV_FILE)
export $(ENV_EXPORTS)

.PHONY: bootstrap nodes linux-pi ssh-setup sudo-setup port-cleanup cleanup-cluster dependency-setup atlasnet-push atlasnet-deploy atlasnet-status atlasnet-merge-manifests

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

linux-pi:
	@: "$${SERVER_IP:?Set SERVER_IP in .env or pass SERVER_IP=...}"
	@: "$${SERVER_SSH_USER:?Set SERVER_SSH_USER in .env or pass SERVER_SSH_USER=...}"
	@: "$${WORKER_IPS:?Set WORKER_IPS in .env or pass WORKER_IPS=\"ip1 ip2\" ...}"; \
	  workers="$$(echo "$$WORKER_IPS" | xargs)"; \
	  ./scripts/port_cleanup.sh; \
	  ./scripts/bootstrap.sh \
	    --server-ip "$$SERVER_IP" \
	    --server-user "$$SERVER_SSH_USER" \
	    --worker-ips "$$workers" \
	    --worker-user "$${WORKER_SSH_USER:-pi}" \
	    --ssh-key "$${SSH_KEY:-$$HOME/.ssh/id_ed25519}" \
	    --use-sudo "$${K3SUP_USE_SUDO:-true}" \
	    --api-endpoint "$${K3S_API_ENDPOINT:-$$SERVER_IP}"

atlasnet-deploy:
	./scripts/deploy_atlasnet.sh

atlasnet-push:
	./scripts/push_atlasnet.sh

atlasnet-merge-manifests:
	./scripts/merge_manifests.sh

atlasnet-status:
	KUBECONFIG="$(CURDIR)/config/kubeconfig" kubectl get nodes -o wide
	KUBECONFIG="$(CURDIR)/config/kubeconfig" kubectl get pods,svc -n "$${ATLASNET_K8S_NAMESPACE:-atlasnet}" -o wide

