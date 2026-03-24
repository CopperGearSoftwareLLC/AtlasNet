# k3d HA Validation

This flow validates the current singleton-first AtlasNet HA model on a `k3d` cluster with `3` servers and `0` agents.

## What This Covers
- `Watchdog`, `Cartograph`, `Proxy`, `LLM`, and `InternalDB` run as single replicas on the server nodes.
- `Cartograph` stays reachable through the ingress hostname even after its hosting node is stopped and the pod is rescheduled.
- `Proxy` keeps a stable external address through the `atlasnet-proxy` `LoadBalancer` service while its singleton pod is recreated on another server.
- `InternalDB` stays on the stable service name `internaldb:6379` and preserves state across an abrupt node stop followed by that node returning.

## Current Recovery Model
- `Cartograph`, `Proxy`, and `Watchdog` are stateless enough to relocate to another k3d server after abrupt node loss.
- `Proxy` now uses a stable logical proxy UUID, and the client reconnect path reuses an existing client identity so routing state can be rebound after reconnect.
- `InternalDB` is intentionally a persistent singleton. In local `k3d`, that means the stable service name is preserved, but recovery depends on the original PVC becoming available again. The validation drill therefore stops the node and then brings the same node back.

## Run The Validation
From the repo root:

```bash
k8s/k3d/scripts/ValidateAtlasNetK3dHA.sh
```

You can override the main addresses the same way the deploy script does:

```bash
ATLASNET_K3D_CARTOGRAPH_INGRESS_HOST=cartograph.k3d.atlasnet.local \
ATLASNET_K3D_PROXY_PUBLIC_HOST=127.0.0.1 \
ATLASNET_K3D_PROXY_PUBLIC_PORT=2555 \
k8s/k3d/scripts/ValidateAtlasNetK3dHA.sh
```

## What The Script Does
1. Verifies that the cluster nodes and singleton workloads are ready.
2. Checks the stable access paths:
   `Cartograph` via ingress, `Proxy` via service endpoint, and `InternalDB` via a Valkey round-trip.
3. Stops the server node currently hosting `Cartograph`, waits for relocation, and re-checks ingress reachability.
4. Stops the server node currently hosting `Watchdog`, waits for relocation, and confirms rollout recovery.
5. Stops the server node currently hosting `Proxy`, waits for relocation, and confirms the service still has an endpoint.
6. Stops the server node currently hosting `InternalDB`, starts it again, and verifies the persistent singleton comes back with data intact.

## Manual Follow-Ups
- Run a real AtlasNet client through the `Proxy` address and confirm reconnect/resume behavior after the proxy node drill.
- Inspect `kubectl get pod -n atlasnet-dev -o wide` before and after the script to confirm singleton placement movement.
- If you want to test longer outages for `InternalDB`, use remote/shared storage before expecting migration to a different server.
