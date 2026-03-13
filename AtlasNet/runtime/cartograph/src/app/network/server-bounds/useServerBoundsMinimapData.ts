'use client';

import { useMemo } from 'react';
import type {
  AuthorityEntityTelemetry,
  ShapeJS,
  ShardTelemetry,
} from '../../shared/cartographTypes';
import {
  getShapeAnchorPoints,
  computeMapBoundsCenter,
  computeNetworkEdgeCount,
  computeProjectedShardPositions,
  computeShardAnchorPositions,
  computeShardGeometryById,
  computeShardHoverBoundsById,
  isShardIdentity,
  normalizeShardId,
  type Point2,
  type ShardHoverBounds,
} from '../../map/core/mapData';
import { buildFramePolygonFromPoints } from '../../map/core/shapeGeometry';
import type { ServerBoundsShardSummary } from './serverBoundsTypes';

interface UseServerBoundsMinimapDataArgs {
  heuristicShapes: ShapeJS[];
  authorityEntities: AuthorityEntityTelemetry[];
  networkTelemetry: ShardTelemetry[];
}

interface ServerBoundsMinimapData {
  shardSummaries: ServerBoundsShardSummary[];
  shardBoundsByIdWithNetworkFallback: Map<string, ShardHoverBounds>;
  shardPolygonsById: Map<string, Point2[][]>;
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
  const shardAnchorPositions = useMemo(
    () => computeShardAnchorPositions(heuristicShapes),
    [heuristicShapes]
  );

  const mapBoundsCenter = useMemo(
    () => computeMapBoundsCenter(heuristicShapes),
    [heuristicShapes]
  );

  const minimapClipPolygon = useMemo(() => {
    const points: Point2[] = [];
    for (const shape of heuristicShapes) {
      points.push(...getShapeAnchorPoints(shape));
    }
    for (const entity of authorityEntities) {
      if (Number.isFinite(entity.x) && Number.isFinite(entity.y)) {
        points.push({ x: entity.x, y: entity.y });
      }
    }
    for (const anchor of shardAnchorPositions.values()) {
      points.push(anchor);
    }

    return buildFramePolygonFromPoints(points, 20);
  }, [authorityEntities, heuristicShapes, shardAnchorPositions]);

  const { shardBoundsById, shardPolygonsById, claimedShardIds } = useMemo(
    () =>
      computeShardGeometryById({
        baseShapes: heuristicShapes,
        clipPolygon: minimapClipPolygon,
      }),
    [heuristicShapes, minimapClipPolygon]
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
    () => new Set(claimedShardIds),
    [claimedShardIds]
  );

  const networkEdgeCount = useMemo(
    () => computeNetworkEdgeCount({ networkNodeIdSet, networkTelemetry }),
    [networkNodeIdSet, networkTelemetry]
  );

  const shardSummaries = useMemo(() => {
    const ids = new Set<string>();
    for (const shardId of claimedBoundShardIds) ids.add(shardId);
    for (const shardId of shardBoundsByIdWithNetworkFallback.keys()) ids.add(shardId);
    for (const shardId of shardTelemetryById.keys()) ids.add(shardId);
    for (const shardId of entitiesByShardId.keys()) ids.add(shardId);

    const out: ServerBoundsShardSummary[] = [];
    for (const shardId of ids) {
      const telemetry = shardTelemetryById.get(shardId);
      const claimedBound = shardBoundsById.get(shardId) ?? null;
      const bounds = shardBoundsByIdWithNetworkFallback.get(shardId) ?? null;
      const hasClaimedBound = claimedBoundShardIds.has(shardId);
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
        avgPingMs: telemetry?.avgPingMs ?? null,
      });
    }

    out.sort((left, right) => left.shardId.localeCompare(right.shardId));
    return out;
  }, [
    clientsByShardId,
    claimedBoundShardIds,
    entitiesByShardId,
    shardBoundsById,
    shardBoundsByIdWithNetworkFallback,
    shardTelemetryById,
  ]);

  return {
    shardSummaries,
    shardBoundsByIdWithNetworkFallback,
    shardPolygonsById,
    claimedBoundShardIds,
    networkNodeIds,
    networkNodeIdSet,
    networkEdgeCount,
  };
}
