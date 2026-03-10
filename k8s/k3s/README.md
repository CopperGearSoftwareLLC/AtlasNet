# k3s slim

Minimal automation to build a small k3s cluster with `k3sup`. Supports an arbitrary number of server and worker nodes; node architecture (amd64, arm64, etc.) does not need to be specified—use multi-arch images so each node pulls the correct image.

## What this does
- Sets up SSH key access to server(s) and worker(s).
- Sets up passwordless sudo for those SSH users (if needed).
- Installs k3s on the first server, joins additional servers (HA) and workers.
- Writes kubeconfig to `~/.kube/config` (or `K3S_KUBECONFIG_PATH` if set).
- Mirrors that kubeconfig to `config/kubeconfig` for project-local Make targets.

## Prereqs
- Run from your Linux control machine.
- Node IPs reachable via SSH.
- `kubectl`, `helm`, and `ssh` tools installed locally.

## 1) Configure `.env`
```bash
cp .env.example .env
```

Edit `.env`:
- `SERVER_IPS` — space-separated server IPs; first is primary.
- `SERVER_SSH_USER` — SSH username on server node(s)
- `WORKER_IPS` — space-separated worker IPs; can be empty for a server-only cluster
- `WORKER_SSH_USER` — SSH username on worker node(s)
- `SSH_KEY` (default is usually fine)
- `K3SUP_USE_SUDO=true` (default)

## 2) Run cluster setup
```bash
make ssh-setup
make sudo-setup
make k3s-deploy
```

`make k3s-deploy` automatically runs a port cleanup step first (`scripts/port_cleanup.sh`).
By default it frees `7946` on server/worker (common Docker Swarm vs MetalLB conflict).
You can override with `SERVER_PORT_CLEANUP_PORTS` / `WORKER_PORT_CLEANUP_PORTS` in `.env`.
Port cleanup will stop a systemd service if it owns the port, otherwise it terminates the process.

## 3) Verify
```bash
kubectl get nodes -o wide
kubectl get pods -A -o wide
```

`kubectl` should work directly because `make k3s-deploy` writes `~/.kube/config`.

## 4) Deploy AtlasNet workloads (Docker Hub images)

Set these in `.env`:
- `DOCKERHUB_NAMESPACE`
- `ATLASNET_IMAGE_TAG`
- Optional image overrides: `ATLASNET_WATCHDOG_IMAGE`, `ATLASNET_PROXY_IMAGE`, `ATLASNET_SANDBOX_SERVER_IMAGE`, `ATLASNET_CARTOGRAPH_IMAGE`

Then deploy:
```bash
make atlasnet-push
make atlasnet-deploy
make atlasnet-status
```

The deploy script reuses the AtlasNet Kubernetes runtime manifest and rewrites images to Docker Hub refs.

`make atlasnet-push` now assumes you have already built local Docker images on your dev machine
(for example via the CMake target `sandbox_atlasnet_run`, which runs `AtlasnetDockerBuild_Fast`
and builds:
- `watchdog:latest`
- `proxy:latest`
- `sandbox-server:latest`
- `cartograph:latest`

`make atlasnet-push` will:
- tag those local images as `${DOCKERHUB_NAMESPACE}/*:${ATLASNET_IMAGE_TAG}`
- push them to Docker Hub

If you need a custom game-server image, set `ATLASNET_SANDBOX_SERVER_IMAGE` explicitly in `.env`.

### Private Docker Hub repos

If images are private, set:
- `DOCKERHUB_REQUIRE_AUTH=true`
- `DOCKERHUB_USERNAME`
- `DOCKERHUB_TOKEN` (Docker Hub access token)

`make atlasnet-deploy` will create/update a pull secret in the target namespace and patch the default service account to use it.

### Mixed-architecture clusters

Use a single multi-arch image tag (e.g. `ATLASNET_IMAGE_TAG=latest` with a manifest that includes both amd64 and arm64). Each node will pull the image for its architecture; you do not need to specify which nodes are amd64 or arm64.

## Make targets
- `make ssh-setup`: create SSH key (if missing) and copy to server(s) + worker(s).
- `make sudo-setup`: enable passwordless sudo for SSH users on server(s) + worker(s).
- `make dependency-setup`: install iptables and set cgroup flags on all nodes (run once; reboot nodes if cmdline changed).
- `make port-cleanup`: free configured required ports on server(s) + worker(s).
- `make k3s-deploy`: install k3s on first server, join additional servers (HA) and workers.
- `make platform`: install optional platform add-ons via `scripts/platform.sh` (metrics-server, MetalLB, ingress-nginx, cert-manager) based on `.env` flags.
- `make nodes`: quick `kubectl get nodes -o wide` using project kubeconfig.
- `make atlasnet-push`: tag and push existing local AtlasNet images to Docker Hub.
- `make atlasnet-merge-manifests`: create multi-arch manifest tag from per-arch tags (see .env `ATLASNET_IMAGE_TAG_*`).
- `make atlasnet-deploy`: apply AtlasNet runtime workloads using Docker Hub images.
- `make atlasnet-status`: show node/pod/service status for AtlasNet namespace.
- `make cleanup-cluster`: uninstall k3s from worker(s) and server(s) and remove local project kubeconfig.

## Troubleshooting
- `Permission denied (publickey,password)`:
  run `make ssh-setup`.
- `sudo: Authentication failed`:
  run `make sudo-setup`, or use root SSH users and set `K3SUP_USE_SUDO=false`.
- `kubectl` tries `localhost:8080`:
  your kubeconfig is not active; rerun `make k3s-deploy` or check `~/.kube/config`.
