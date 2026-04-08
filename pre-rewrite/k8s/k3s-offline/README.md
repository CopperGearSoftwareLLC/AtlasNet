# k3s-offline

Make-only workflow for preparing a fully offline AtlasNet k3s deployment.

This directory is intentionally separate from `k8s/k3s`. Use `k8s/k3s` for the simple online flow; use `k8s/k3s-offline` when you want to:
- build AtlasNet images on separate `amd64` and `arm64` machines,
- vendor k3s/chart artifacts while online,
- seed a LAN registry on the control machine while online,
- seed the cluster nodes once while online,
- later deploy the cluster and AtlasNet workloads on an isolated Ethernet network with no internet access.

## What this does
- publishes AtlasNet images under arch-specific tags,
- optionally merges those tags into one multi-arch deployment tag,
- creates a versioned offline bundle on the control machine,
- starts a local Docker registry on the control machine and seeds it with the platform and AtlasNet images,
- seeds each node with the bundle assets for its architecture,
- installs k3s from vendored air-gap assets,
- configures each node with `registries.yaml` so `k3s/containerd` pulls through the LAN registry,
- installs platform add-ons from local chart archives,
- deploys AtlasNet from the shared `k8s/charts/atlasnet` chart using normal image refs that are mirrored to the LAN registry.

## Control machine prerequisites
- Linux control machine with `docker`, `helm`, `kubectl`, `ssh`, `scp`, `curl`, `tar`, and `sha256sum`
- reachable node IPs over SSH while doing the online prep phase
- local repo checkout that includes the built AtlasNet images only if this machine is also used for `make atlasnet-push`

Nodes do not need Docker. They only need SSH access and Debian/Ubuntu-style `apt-get` during the one-time online `make dependency-setup`.

## Configure `.env`
```bash
cp .env.example .env
```

Pin the values before building the bundle:
- `K3S_VERSION`
- `ATLASNET_IMAGE_TAG`, `ATLASNET_IMAGE_TAG_AMD64`, `ATLASNET_IMAGE_TAG_ARM64`
- `ATLASNET_INTERNALDB_IMAGE`
- chart versions
- if you explicitly enable the in-cluster LLM pod, the offline LLM model file at `./models/${ATLASNET_LLM_MODEL_FILE_NAME}`

## Workflow

### 1. Build and push AtlasNet images on each architecture machine
On the `amd64` machine:
```bash
make atlasnet-push
```

On the `arm64` machine:
```bash
make atlasnet-push
```

`make atlasnet-push` tags the already-built local images as:
- `${DOCKERHUB_NAMESPACE}/watchdog:${ATLASNET_IMAGE_TAG_AMD64|ARM64}`
- `${DOCKERHUB_NAMESPACE}/proxy:${ATLASNET_IMAGE_TAG_AMD64|ARM64}`
- `${DOCKERHUB_NAMESPACE}/sandbox-server:${ATLASNET_IMAGE_TAG_AMD64|ARM64}`
- `${DOCKERHUB_NAMESPACE}/cartograph:${ATLASNET_IMAGE_TAG_AMD64|ARM64}`

### 2. Online on the control machine
Optionally merge the arch-specific tags under a single deployment tag:
```bash
make atlasnet-merge-manifests
```

Build the offline bundle:
```bash
make offline-bundle
```

`make offline-bundle` now does three things:
- vendors k3s install assets,
- vendors the Helm chart archives,
- starts the local registry and seeds it with every platform and AtlasNet image the cluster will need.

If you explicitly enable the in-cluster LLM pod, `make offline-bundle` will also build the model-seed image from:
- `./models/Qwen3-1.7B-seed_gen_voronoi-Q4_K_M.gguf`

If that file is missing during the online prep phase, `make offline-bundle` will download it from `ATLASNET_LLM_MODEL_URL` first.

Set up node access and seed them:
```bash
make ssh-setup
make sudo-setup
make dependency-setup
```

### 3. Offline on the isolated work network
On the control machine, start the host-side Docker Model Runner that watchdog will call:
```bash
make llm-runner
```

After bringing the control machine and seeded nodes to the offline Ethernet network:
```bash
make k3s-deploy
make platform
make atlasnet-deploy
```

Or run the whole offline deployment sequence in one step:
```bash
make cluster-and-deploy
```

For live node-level pod log following on every cluster node:
```bash
make node-logs-start
make node-logs-stop
```

### 4. Verify
```bash
make nodes
make atlasnet-status
KUBECONFIG="$(pwd)/config/kubeconfig" kubectl get pods -A -o wide
```

## Bundle layout
The default bundle directory is `./offline-bundle` and contains:
- `bundle-manifest.env`
- `checksums.txt`
- `tools/k3sup`
- `k3s/install.sh`
- `k3s/<arch>/k3s`
- `k3s/<arch>/k3s-airgap-images-<arch>.tar.zst`
- `charts/*.tgz`
- `platform-image-sources.txt`
- `registry-warnings.txt` when image seeding had warnings

## Notes
- By default, `k3s-offline` does not deploy an in-cluster LLM pod. Watchdog points at a Docker Model Runner process on the control host using the same `ATLASNET_LLM_ENDPOINT` contract as the Swarm flow.
- To make that host-side model runner reachable from inside pods, `atlasnet-deploy` creates a small host-network proxy deployment and service (`atlasnet-llm-host-proxy`) on the control-plane node. If you change the proxy image or enable this mode for the first time, rerun `make offline-bundle` while online so the image is mirrored into the LAN registry.
- `make atlasnet-merge-manifests` is optional for offline deployment. Registry seeding can build the final mirrored deployment tag from the per-arch tags directly.
- A bundle can still be usable for mono-architecture testing even when another architecture's AtlasNet images are missing. Registry seeding will warn and continue for the missing architecture, but those nodes will not be able to run AtlasNet workloads until reseeded.
- If you change any pinned version, image tag, or, for the optional in-cluster LLM path, the LLM model file, rebuild the bundle and rerun `make dependency-setup`.
- `cartograph.atlasnet.local` will not resolve automatically on an isolated LAN. After deploy, use the printed ingress IP and add an `/etc/hosts` entry on client machines.
- The registry-based flow follows the K3s private-registry pattern: nodes get `/etc/rancher/k3s/registries.yaml`, and K3s is installed with `--disable-default-registry-endpoint` so an offline node will not fall back to the public internet.
- `cert-manager` can be installed offline, but public ACME issuance is not expected to work on an isolated network.
- `make node-logs-start` mirrors the online `k3s` workflow and tails `/var/log/pods/*.log` on each node over SSH. `make node-logs-stop` stops those followers.
