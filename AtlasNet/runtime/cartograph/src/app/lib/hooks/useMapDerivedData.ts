import { useMemo } from 'react';
import type {
  AuthorityEntityTelemetry,
  ShapeJS,
  ShardTelemetry,
} from '../cartographTypes';
import {
  buildHoveredShardEdgeLabels,
  buildOverlayShapes,
  buildShardTelemetryById,
  computeMapBoundsCenter,
  computeNetworkEdgeCount,
  computeNetworkNodeIds,
  computeOwnerPositions,
  computeProjectedShardPositions,
  computeShardAnchorPositions,
  computeShardBoundsById,
  computeShardHoverBoundsById,
  type HoveredShardEdgeLabel,
  type ShardHoverBounds,
} from '../mapData';

interface UseMapDerivedDataArgs {
  baseShapes: ShapeJS[];
  networkTelemetry: ShardTelemetry[];
  authorityEntities: AuthorityEntityTelemetry[];
  showAuthorityEntities: boolean;
  showGnsConnections: boolean;
  hoveredShardId: string | null;
}

interface MapDerivedData {
  combinedShapes: ShapeJS[];
  hoveredShardEdgeLabels: HoveredShardEdgeLabel[];
  networkEdgeCount: number;
  networkNodeIds: string[];
  networkNodeIdSet: Set<string>;
  shardHoverBoundsById: Map<string, ShardHoverBounds>;
  shardTelemetryById: Map<string, ShardTelemetry>;
}

export function useMapDerivedData({
  authorityEntities,
  baseShapes,
  hoveredShardId,
  networkTelemetry,
  showAuthorityEntities,
  showGnsConnections,
}: UseMapDerivedDataArgs): MapDerivedData {
  const ownerPositions = useMemo(
    () => computeOwnerPositions(authorityEntities),
    [authorityEntities]
  );

  const shardAnchorPositions = useMemo(
    () => computeShardAnchorPositions(baseShapes),
    [baseShapes]
  );

  const mapBoundsCenter = useMemo(
    () => computeMapBoundsCenter(baseShapes),
    [baseShapes]
  );

  const shardBoundsById = useMemo(
    () => computeShardBoundsById(baseShapes),
    [baseShapes]
  );

  const networkNodeIds = useMemo(
    () => computeNetworkNodeIds(networkTelemetry),
    [networkTelemetry]
  );

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

  const shardHoverBoundsById = useMemo(
    () =>
      computeShardHoverBoundsById({
        networkNodeIds,
        projectedShardPositions,
        shardBoundsById,
      }),
    [networkNodeIds, projectedShardPositions, shardBoundsById]
  );

  const networkEdgeCount = useMemo(
    () => computeNetworkEdgeCount({ networkNodeIdSet, networkTelemetry }),
    [networkNodeIdSet, networkTelemetry]
  );

  const shardTelemetryById = useMemo(
    () => buildShardTelemetryById(networkTelemetry),
    [networkTelemetry]
  );

  const hoveredShardEdgeLabels = useMemo(
    () =>
      buildHoveredShardEdgeLabels({
        hoveredShardId,
        networkNodeIdSet,
        projectedShardPositions,
        shardTelemetryById,
      }),
    [hoveredShardId, networkNodeIdSet, projectedShardPositions, shardTelemetryById]
  );

  const overlayShapes = useMemo(
    () =>
      buildOverlayShapes({
        authorityEntities,
        networkTelemetry,
        networkNodeIds,
        networkNodeIdSet,
        ownerPositions,
        projectedShardPositions,
        showAuthorityEntities,
        showGnsConnections,
      }),
    [
      authorityEntities,
      networkTelemetry,
      networkNodeIds,
      networkNodeIdSet,
      ownerPositions,
      projectedShardPositions,
      showAuthorityEntities,
      showGnsConnections,
    ]
  );

  const combinedShapes = useMemo(
    () => [...baseShapes, ...overlayShapes],
    [baseShapes, overlayShapes]
  );

  return {
    combinedShapes,
    hoveredShardEdgeLabels,
    networkEdgeCount,
    networkNodeIds,
    networkNodeIdSet,
    shardHoverBoundsById,
    shardTelemetryById,
  };
}
