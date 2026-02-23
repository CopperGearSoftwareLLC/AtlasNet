'use client';

import { useMemo } from 'react';
import type {
  AuthorityEntityTelemetry,
  ShapeJS,
  ShardTelemetry,
} from '../cartographTypes';
import {
  computeMapBoundsCenter,
  computeNetworkEdgeCount,
  computeProjectedShardPositions,
  computeShardAnchorPositions,
  computeShardBoundsById,
  computeShardHoverBoundsById,
  isShardIdentity,
  normalizeShardId,
  type ShardHoverBounds,
} from '../mapData';
import type { ServerBoundsShardSummary } from '../serverBoundsTypes';

interface UseServerBoundsMinimapDataArgs {
  heuristicShapes: ShapeJS[];
  authorityEntities: AuthorityEntityTelemetry[];
  networkTelemetry: ShardTelemetry[];
}

interface ServerBoundsMinimapData {
  shardSummaries: ServerBoundsShardSummary[];
  shardBoundsByIdWithNetworkFallback: Map<string, ShardHoverBounds>;
  claimedBoundShardIds: Set<string>;
  networkNodeIds: string[];
  networkNodeIdSet: Set<string>;
  networkEdgeCount: number;
}

export function useServerBoundsMinimapData({
  heuristicShapes,
  authorityEntities,
  networkTelemetry,
}: UseServerBoundsMinimapDataArgs): ServerBoundsMinimapData {
  const shardBoundsById = useMemo(
    () => computeShardBoundsById(heuristicShapes),
    [heuristicShapes]
  );

  const shardAnchorPositions = useMemo(
    () => computeShardAnchorPositions(heuristicShapes),
    [heuristicShapes]
  );

  const mapBoundsCenter = useMemo(
    () => computeMapBoundsCenter(heuristicShapes),
    [heuristicShapes]
  );

  const shardTelemetryById = useMemo(() => {
    const out = new Map<string, (typeof networkTelemetry)[number]>();
    for (const shard of networkTelemetry) {
      const shardId = normalizeShardId(shard.shardId);
      if (!shardId) {
        continue;
      }
      out.set(shardId, shard);
    }
    return out;
  }, [networkTelemetry]);

  const entitiesByShardId = useMemo(() => {
    const out = new Map<string, number>();
    for (const entity of authorityEntities) {
      const shardId = normalizeShardId(entity.ownerId);
      if (!isShardIdentity(shardId)) {
        continue;
      }
      out.set(shardId, (out.get(shardId) ?? 0) + 1);
    }
    return out;
  }, [authorityEntities]);

  const clientsByShardId = useMemo(() => {
    const out = new Map<string, number>();
    for (const entity of authorityEntities) {
      if (!entity.isClient) {
        continue;
      }
      const shardId = normalizeShardId(entity.ownerId);
      if (!isShardIdentity(shardId)) {
        continue;
      }
      out.set(shardId, (out.get(shardId) ?? 0) + 1);
    }
    return out;
  }, [authorityEntities]);

  const networkNodeIds = useMemo(() => {
    const out: string[] = [];
    const seen = new Set<string>();
    for (const shard of networkTelemetry) {
      const shardId = normalizeShardId(shard.shardId);
      if (!shardId || seen.has(shardId)) {
        continue;
      }
      seen.add(shardId);
      out.push(shardId);
    }
    return out;
  }, [networkTelemetry]);

  const networkNodeIdSet = useMemo(
    () => new Set(networkNodeIds),
    [networkNodeIds]
  );

  const projectedShardPositions = useMemo(
    () =>
      computeProjectedShardPositions({
        mapBoundsCenter,
        networkNodeIds,
        shardAnchorPositions,
      }),
    [mapBoundsCenter, networkNodeIds, shardAnchorPositions]
  );

  const shardBoundsByIdWithNetworkFallback = useMemo(
    () =>
      computeShardHoverBoundsById({
        networkNodeIds,
        projectedShardPositions,
        shardBoundsById,
      }),
    [networkNodeIds, projectedShardPositions, shardBoundsById]
  );

  const claimedBoundShardIds = useMemo(
    () => new Set(shardBoundsById.keys()),
    [shardBoundsById]
  );

  const networkEdgeCount = useMemo(
    () => computeNetworkEdgeCount({ networkNodeIdSet, networkTelemetry }),
    [networkNodeIdSet, networkTelemetry]
  );

  const shardSummaries = useMemo(() => {
    const ids = new Set<string>();
    for (const shardId of shardBoundsByIdWithNetworkFallback.keys()) ids.add(shardId);
    for (const shardId of shardTelemetryById.keys()) ids.add(shardId);
    for (const shardId of entitiesByShardId.keys()) ids.add(shardId);

    const out: ServerBoundsShardSummary[] = [];
    for (const shardId of ids) {
      const telemetry = shardTelemetryById.get(shardId);
      const claimedBound = shardBoundsById.get(shardId) ?? null;
      const bounds = shardBoundsByIdWithNetworkFallback.get(shardId) ?? null;
      const hasClaimedBound = claimedBound != null;
      const hasNetworkTelemetry = telemetry != null;
      const status: ServerBoundsShardSummary['status'] = hasClaimedBound
        ? hasNetworkTelemetry
          ? 'bounded'
          : 'bounded stale'
        : 'unbounded';
      out.push({
        shardId,
        bounds,
        status,
        hasClaimedBound,
        area: claimedBound?.area ?? 0,
        clientCount: clientsByShardId.get(shardId) ?? 0,
        entityCount: entitiesByShardId.get(shardId) ?? 0,
        connectionCount: telemetry?.connections.length ?? 0,
        downloadKbps: telemetry?.downloadKbps ?? 0,
        uploadKbps: telemetry?.uploadKbps ?? 0,
      });
    }

    out.sort((left, right) => left.shardId.localeCompare(right.shardId));
    return out;
  }, [
    clientsByShardId,
    entitiesByShardId,
    shardBoundsById,
    shardBoundsByIdWithNetworkFallback,
    shardTelemetryById,
  ]);

  return {
    shardSummaries,
    shardBoundsByIdWithNetworkFallback,
    claimedBoundShardIds,
    networkNodeIds,
    networkNodeIdSet,
    networkEdgeCount,
  };
}
