SHELL := /usr/bin/env bash

ENV_FILE ?= .env
ENV_EXPORTS := SERVER_IP SERVER_SSH_USER WORKER_IPS PI_WORKER_IP WORKER_SSH_USER SSH_KEY K3S_API_ENDPOINT K3SUP_USE_SUDO NAMESPACE IMAGE_REPO IMAGE_TAG IMAGE_PLATFORMS IMAGE_BUILD_PLATFORM BUILD_DIR BAKE_FILE BUILDER_NAME WITH_LATEST_TAG TOOLCHAIN_FILE IMAGE_PULL_SECRET_NAME CREATE_PULL_SECRET REGISTRY_SERVER REGISTRY_USERNAME REGISTRY_PASSWORD REGISTRY_EMAIL PROXY_PUBLIC_IP PROXY_PUBLIC_PORT IMAGE_VALKEY KUBECTL_CONTEXT PREBUILT_BAKE_FILE PREBUILT_BUILD_DIR PREBUILT_WEB_NODE_PATH PREBUILT_IMAGE_PLATFORM

-include $(ENV_FILE)
export $(ENV_EXPORTS)

.PHONY: bootstrap platform nodes linux linux-pi ssh-setup sudo-setup port-cleanup cleanup-cluster images publish images-prebuilt publish-prebuilt deploy up up-prebuilt status

bootstrap:
	./scripts/bootstrap.sh

platform:
	./scripts/platform.sh

nodes:
	KUBECONFIG="$(CURDIR)/config/kubeconfig" kubectl get nodes -o wide

ssh-setup:
	./scripts/ssh_setup.sh

sudo-setup:
	./scripts/sudo_setup.sh

port-cleanup:
	./scripts/port_cleanup.sh

cleanup-cluster:
	./scripts/cleanup_cluster.sh

images:
	./scripts/images.sh

publish:
	./scripts/publish.sh

images-prebuilt:
	./scripts/images_prebuilt.sh

publish-prebuilt:
	./scripts/publish_prebuilt.sh

deploy:
	./scripts/deploy.sh

up: publish deploy

up-prebuilt: publish-prebuilt deploy

status:
	KUBECONFIG="$(CURDIR)/config/kubeconfig" kubectl -n "$${NAMESPACE:-atlasnet}" get deploy,svc,pods -o wide

linux:
	@: "$${SERVER_IP:?Set SERVER_IP in .env or pass SERVER_IP=...}"
	@: "$${SERVER_SSH_USER:?Set SERVER_SSH_USER in .env or pass SERVER_SSH_USER=...}"
	./scripts/port_cleanup.sh
	./scripts/bootstrap.sh \
	  --server-ip "$$SERVER_IP" \
	  --server-user "$$SERVER_SSH_USER" \
	  --worker-ips "$${WORKER_IPS:-$${PI_WORKER_IP:-}}" \
	  --worker-user "$${WORKER_SSH_USER:-pi}" \
	  --ssh-key "$${SSH_KEY:-$$HOME/.ssh/id_ed25519}" \
	  --use-sudo "$${K3SUP_USE_SUDO:-true}" \
	  --api-endpoint "$${K3S_API_ENDPOINT:-$$SERVER_IP}"

# Backward-compatible alias.
linux-pi: linux

#############
# one-time per machine/session for Docker Hub push
# docker login docker.io -u dannys0n

# # one-time cluster bootstrap
# make ssh-setup
# make sudo-setup
# make linux
# make platform
# Per deploy iteration (your VSCode/CMake dev flow)
# 
# In VSCode, build your normal targets (sandbox_atlasnet_run and ensure Web.node is built).
# make up-prebuilt
# make status
# make up-prebuilt = make publish-prebuilt + make deploy, and image tag auto-uses current git short SHA if IMAGE_TAG is empty.
# 
# If you want a local image check before push:
# 
# bash
# 
# make images-prebuilt
# make publish-prebuilt
# make deploy
# make status