# Kubernetes manifests

Put any manifests you want to deploy under this folder.

Example:
```bash
export KUBECONFIG="$(pwd)/../config/kubeconfig"
kubectl apply -f examples/hello/
kubectl get deploy,svc -n default
```

AtlasNet runtime deploy is handled by:
```bash
make atlasnet-deploy
```
from the `deploy/k3s_slim` folder.
