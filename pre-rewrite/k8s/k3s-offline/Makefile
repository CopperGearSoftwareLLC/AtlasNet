SHELL := /usr/bin/env bash

ENV_FILE ?= .env
ENV_EXPORTS := SERVER_IPS SERVER_SSH_USER WORKER_IPS WORKER_SSH_USER SSH_KEY K3S_API_ENDPOINT K3S_EXTRA_ARGS K3S_AGENT_EXTRA_ARGS K3S_VERSION K3SUP_VERSION SERVER_NODE_NAME SERVER_PORT_CLEANUP_PORTS WORKER_PORT_CLEANUP_PORTS ATLASNET_K8S_NAMESPACE ATLASNET_HELM_RELEASE_NAME DOCKERHUB_NAMESPACE ATLASNET_IMAGE_TAG ATLASNET_IMAGE_TAG_AMD64 ATLASNET_IMAGE_TAG_ARM64 ATLASNET_WATCHDOG_IMAGE ATLASNET_WATCHDOG_IMAGE_AMD64 ATLASNET_WATCHDOG_IMAGE_ARM64 ATLASNET_PROXY_IMAGE ATLASNET_PROXY_IMAGE_AMD64 ATLASNET_PROXY_IMAGE_ARM64 ATLASNET_SANDBOX_SERVER_IMAGE ATLASNET_SANDBOX_SERVER_IMAGE_AMD64 ATLASNET_SANDBOX_SERVER_IMAGE_ARM64 ATLASNET_CARTOGRAPH_IMAGE ATLASNET_CARTOGRAPH_IMAGE_AMD64 ATLASNET_CARTOGRAPH_IMAGE_ARM64 ATLASNET_INTERNALDB_IMAGE ATLASNET_IMAGE_PULL_POLICY ATLASNET_CARTOGRAPH_INGRESS_CLASS_NAME ATLASNET_CARTOGRAPH_INGRESS_HOST ATLASNET_MULTIARCH_PLATFORMS ATLASNET_OFFLINE_ARCHES ATLASNET_OFFLINE_BUNDLE_DIR ATLASNET_OFFLINE_REMOTE_DIR ATLASNET_OFFLINE_REGISTRY_HOST ATLASNET_OFFLINE_REGISTRY_PORT ATLASNET_OFFLINE_REGISTRY_LOCAL_HOST ATLASNET_OFFLINE_REGISTRY_CONTAINER_NAME ATLASNET_OFFLINE_REGISTRY_DATA_DIR METALLB_ADDRESS_POOL INSTALL_METRICS_SERVER INSTALL_METALLB INSTALL_INGRESS_NGINX INSTALL_CERT_MANAGER METRICS_SERVER_CHART_VERSION METALLB_CHART_VERSION INGRESS_NGINX_CHART_VERSION CERT_MANAGER_CHART_VERSION ATLASNET_K8S_LLM_ENABLED ATLASNET_LLM_ENDPOINT ATLASNET_LLM_DOCKER_HOST ATLASNET_LLM_DOCKER_PORT ATLASNET_LLM_MODEL_REF ATLASNET_LLM_HOST_PROXY_ENABLED ATLASNET_LLM_HOST_PROXY_IMAGE ATLASNET_LLM_HOST_PROXY_NAME ATLASNET_LLM_HOST_PROXY_SERVICE_PORT ATLASNET_LLM_HOST_PROXY_TARGET_PORT ATLASNET_LLM_IMAGE ATLASNET_LLM_SERVICE_PORT ATLASNET_LLM_API_FORMAT ATLASNET_LLM_MODEL_ID ATLASNET_LLM_MODEL_URL ATLASNET_LLM_MODEL_FILE_NAME ATLASNET_LLM_MODEL_FILE_PATH ATLASNET_LLM_MODEL_SEED_IMAGE ATLASNET_LLM_CONTEXT_SIZE ATLASNET_LLM_THREADS

-include $(ENV_FILE)
export $(ENV_EXPORTS)

.PHONY: ssh-setup sudo-setup offline-bundle registry-start registry-stop registry-seed dependency-setup port-cleanup k3s-deploy platform node-logs-start node-logs-stop cluster-and-deploy atlasnet-push atlasnet-merge-manifests atlasnet-deploy atlasnet-status nodes cleanup-cluster llm-runner

ssh-setup:
	./scripts/ssh_setup.sh

sudo-setup:
	./scripts/sudo_setup.sh

offline-bundle:
	./scripts/build_offline_bundle.sh
	$(MAKE) registry-start
	./scripts/registry_seed.sh

registry-start:
	./scripts/registry_start.sh

registry-stop:
	./scripts/registry_stop.sh

registry-seed:
	./scripts/registry_seed.sh

dependency-setup:
	./scripts/dependency_setup.sh

port-cleanup:
	./scripts/port_cleanup.sh

k3s-deploy:
	./scripts/port_cleanup.sh
	./scripts/bootstrap.sh

platform:
	$(MAKE) registry-start
	./scripts/platform.sh

node-logs-start:
	./scripts/node_logs.sh start

node-logs-stop:
	./scripts/node_logs.sh stop

cluster-and-deploy:
	$(MAKE) k3s-deploy
	$(MAKE) platform
	$(MAKE) atlasnet-deploy

atlasnet-push:
	./scripts/push_atlasnet.sh

atlasnet-merge-manifests:
	./scripts/merge_manifests.sh

atlasnet-deploy:
	$(MAKE) registry-start
	./scripts/deploy_atlasnet.sh

atlasnet-status:
	KUBECONFIG="$(CURDIR)/config/kubeconfig" kubectl get nodes -o wide
	KUBECONFIG="$(CURDIR)/config/kubeconfig" kubectl get pods,svc,ingress -n "$${ATLASNET_K8S_NAMESPACE:-atlasnet}" -o wide

nodes:
	KUBECONFIG="$(CURDIR)/config/kubeconfig" kubectl get nodes -o wide

cleanup-cluster:
	./scripts/cleanup_cluster.sh

llm-runner:
	../../scripts/run_docker_model_runner.sh "$${ATLASNET_LLM_MODEL_REF:-huggingface.co/Dannys0n/Qwen3-1.7B-seed_gen_voronoi:Q4_K_M}"

# make atlasnet-push
# make offline-bundle
# make ssh-setup
# make sudo-setup
# make dependency-setup

# make k3s-deploy
# make platform
# make atlasnet-deploy
# or----
# make cluster-and-deploy

# make cleanup-cluster

# node-logs-start
# node-logs-stop
