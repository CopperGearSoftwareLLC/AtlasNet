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

# run in this order:
# ssh-setup
# sudo-setup
# linux

#########################

#Run these from repo root:

#cd /home/danny/Desktop/AtlasNet/deploy/k3s_slim
#First-time (cluster + build + publish + deploy):
#
#docker login <registry>
#make ssh-setup
#make sudo-setup
#make linux
#make images
#make publish
#make deploy
#make status
#If cluster is already up, just refresh images + rollout:
#
#cd /home/danny/Desktop/AtlasNet/deploy/k3s_slim
#docker login <registry>
#make up
#make status
#If your registry is private, set these in deploy/k3s_slim/.env before deploy:
#
#CREATE_PULL_SECRET=true
#REGISTRY_PASSWORD=<your token>
#(and username/server values already in there)
