# k3s slim (Linux server + workers)

Minimal automation to build a small k3s cluster with `k3sup`.

## What this does
- Sets up SSH key access to server and worker nodes.
- Sets up passwordless sudo for those SSH users (if needed).
- Installs k3s server and joins worker nodes.
- Writes kubeconfig to `config/kubeconfig`.
- Automatically symlinks `~/.kube/config` to that file (backs up existing non-symlink config first).

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
- `WORKER_IPS` (space/comma-separated worker IPs)
- Optional legacy single-worker fallback: `PI_WORKER_IP`
- `WORKER_SSH_USER` (SSH username on the worker; this is not hostname)
- `SSH_KEY` (default is usually fine)
- `K3SUP_USE_SUDO=true` (default)

## 2) Run cluster setup
```bash
make ssh-setup
make sudo-setup
make linux
```

`make linux` automatically runs a port cleanup step first (`scripts/port_cleanup.sh`).
By default it frees `7946` on server/workers (common Docker Swarm vs MetalLB conflict).
You can override with `SERVER_PORT_CLEANUP_PORTS` / `WORKER_PORT_CLEANUP_PORTS` in `.env`.
Port cleanup will stop a systemd service if it owns the port, otherwise it terminates the process.
By default it also checks server API port `6443` for conflicts (`SERVER_CLEAN_K3S_API_PORT=true`) while preserving an existing `k3s` listener unless you explicitly set `SERVER_STOP_K3S_ON_PORT_CLEANUP=true`.

## 3) Verify
```bash
kubectl get nodes -o wide
kubectl get pods -A -o wide
```

`kubectl` should work directly because `make linux` links `~/.kube/config` for you.

## 4) Build/publish/deploy AtlasNet

Configure AtlasNet variables in `.env`:

- `IMAGE_REPO` (required)
- `IMAGE_TAG` (optional; leave empty to auto-use git short SHA)
- `IMAGE_PLATFORMS` (publish platforms; defaults to `linux/amd64,linux/arm64`)
- `IMAGE_BUILD_PLATFORM` (optional; local platform for `make images`, defaults to first entry in `IMAGE_PLATFORMS`)
- `NAMESPACE` (default `atlasnet`)

Build local images (no push):

```bash
make images
```

Notes:
- This target wraps `Dev/BuildAndPushAtlasNetImages.sh`.
- If `BAKE_FILE=docker/dockerfiles/docker-bake.json`, it skips local stage build and compiles inside Docker.
- If `BAKE_FILE=docker/dockerfiles/docker-bake.copy.json`, it performs a fresh local stage build first.
- If `IMAGE_TAG` is empty, the script auto-picks git short SHA and records it for publish/deploy reuse.

Publish multi-arch images:

```bash
make publish
```

`make publish` uses the same bake-driven behavior as `make images`, then pushes to the registry.

Deploy AtlasNet to this cluster:

```bash
make deploy
```

Publish + deploy:

```bash
make up
```

Check deployment status:

```bash
make status
```

If your registry is private, set:

- `IMAGE_PULL_SECRET_NAME`
- `CREATE_PULL_SECRET=true`
- `REGISTRY_SERVER`
- `REGISTRY_USERNAME`
- `REGISTRY_PASSWORD`

Then run `make deploy` (the secret is created/updated automatically).

## Optional add-ons
```bash
make platform
```

This uses `.env` flags for metrics-server / MetalLB / ingress / cert-manager.
Legacy `config/cluster.env` values are also supported if the file exists.

## Make targets
- `make ssh-setup`: create SSH key (if missing) and copy to server + workers.
- `make sudo-setup`: enable passwordless sudo for SSH users on server + workers.
- `make port-cleanup`: free configured required ports on server + workers.
- `make linux`: install k3s server and join workers.
- `make linux-pi`: legacy alias for `make linux`.
- `make nodes`: quick `kubectl get nodes -o wide` using project kubeconfig.
- `make platform`: install optional platform add-ons.
- `make cleanup-cluster`: uninstall k3s from workers/server and remove local project kubeconfig.
- `make images`: build all AtlasNet images locally (no push).
- `make publish`: build and publish AtlasNet images.
- `make deploy`: deploy AtlasNet manifests to this cluster.
- `make up`: run publish + deploy in sequence.
- `make status`: show AtlasNet deployments/services/pods.

## Troubleshooting
- `Permission denied (publickey,password)`:
  run `make ssh-setup`.
- `sudo: Authentication failed`:
  run `make sudo-setup`, or use root SSH users and set `K3SUP_USE_SUDO=false`.
- `kubectl` tries `localhost:8080`:
  your kubeconfig is not active; rerun `make linux` or check `~/.kube/config` symlink.
