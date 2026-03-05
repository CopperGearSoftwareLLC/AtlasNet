# k3s slim (Linux server + Pi worker)

Minimal automation to build a small k3s cluster with `k3sup`.

## What this does
- Sets up SSH key access to server and worker.
- Sets up passwordless sudo for those SSH users (if needed).
- Installs k3s server and joins worker.
- Writes kubeconfig directly to `~/.kube/config` (or `K3S_KUBECONFIG_PATH` if set).
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
- `SERVER_IP`
- `SERVER_SSH_USER`
- `WORKER_IPS` (space-separated worker IPs, e.g. Linux agent + Pi agent)
- `WORKER_SSH_USER` (SSH username used on workers)
- `SSH_KEY` (default is usually fine)
- `K3SUP_USE_SUDO=true` (default)

Legacy fallback still works when `WORKER_IPS` is empty:
- `LINUX_WORKER_IP`
- `PI_WORKER_IP`

## 2) Run cluster setup
```bash
make ssh-setup
make sudo-setup
make linux-pi
```

`make linux-pi` automatically runs a port cleanup step first (`scripts/port_cleanup.sh`).
By default it frees `7946` on server/worker (common Docker Swarm vs MetalLB conflict).
You can override with `SERVER_PORT_CLEANUP_PORTS` / `WORKER_PORT_CLEANUP_PORTS` in `.env`.
Port cleanup will stop a systemd service if it owns the port, otherwise it terminates the process.

## 3) Verify
```bash
kubectl get nodes -o wide
kubectl get pods -A -o wide
```

`kubectl` should work directly because `make linux-pi` writes `~/.kube/config`.

## 4) Deploy AtlasNet workloads (Docker Hub images)

Set these in `.env`:
- `DOCKERHUB_NAMESPACE`
- `ATLASNET_IMAGE_TAG`
- Optional image overrides: `ATLASNET_WATCHDOG_IMAGE`, `ATLASNET_PROXY_IMAGE`, `ATLASNET_SHARD_IMAGE`, `ATLASNET_CARTOGRAPH_IMAGE`

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
- `shard:latest`
- `cartograph:latest`

`make atlasnet-push` will:
- tag those local images as `${DOCKERHUB_NAMESPACE}/*:${ATLASNET_IMAGE_TAG}`
- push them to Docker Hub

If you need a custom shard/game-server image, set `ATLASNET_SHARD_IMAGE` explicitly in `.env`.

### Private Docker Hub repos

If images are private, set:
- `DOCKERHUB_REQUIRE_AUTH=true`
- `DOCKERHUB_USERNAME`
- `DOCKERHUB_TOKEN` (Docker Hub access token)

`make atlasnet-deploy` will create/update a pull secret in the target namespace and patch the default service account to use it.

### Linux + Pi mixed cluster note

If your cluster has both `amd64` (Linux) and `arm64` (Pi) workers, Docker Hub images must be multi-arch manifests.
If not, pods may fail with `ImagePullBackOff` or architecture errors.

## Make targets
- `make ssh-setup`: create SSH key (if missing) and copy to server + worker.
- `make sudo-setup`: enable passwordless sudo for SSH users on server + workers.
- `make port-cleanup`: free configured required ports on server + workers.
- `make linux-pi`: install k3s server and join worker nodes.
- `make nodes`: quick `kubectl get nodes -o wide` using project kubeconfig.
- `make atlasnet-push`: build and push multi-arch runtime images to Docker Hub.
- `make atlasnet-deploy`: apply AtlasNet runtime workloads using Docker Hub images.
- `make atlasnet-status`: show node/pod/service status for AtlasNet namespace.
- `make cleanup-cluster`: uninstall k3s from worker/server and remove local project kubeconfig.

## Troubleshooting
- `Permission denied (publickey,password)`:
  run `make ssh-setup`.
- `sudo: Authentication failed`:
  run `make sudo-setup`, or use root SSH users and set `K3SUP_USE_SUDO=false`.
- `kubectl` tries `localhost:8080`:
  your kubeconfig is not active; rerun `make linux-pi` or check `~/.kube/config`.
