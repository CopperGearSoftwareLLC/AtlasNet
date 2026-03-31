import { useMemo } from 'react';
import type {
  AuthorityLinkMode,
  AuthorityEntityTelemetry,
  ShapeJS,
  ShardTelemetry,
  TransferManifestTelemetry,
} from '../../shared/cartographTypes';
import {
  buildHoveredShardEdgeLabels,
  buildFilteredBaseShapes,
  buildOverlayShapes,
  buildShardTelemetryById,
  computeMapBoundsCenter,
  computeNetworkEdgeCount,
  computeNetworkNodeIds,
  computeOwnerPositions,
  computeProjectedShardPositions,
  computeShardAnchorPositions,
  computeShardGeometryById,
  computeShardHoverBoundsById,
  type HoveredShardEdgeLabel,
  type ShardHoverBounds,
  type ShardHoverRegion,
} from './mapData';

interface UseMapDerivedDataArgs {
  baseShapes: ShapeJS[];
  networkTelemetry: ShardTelemetry[];
  authorityEntities: AuthorityEntityTelemetry[];
  authorityLinkMode: AuthorityLinkMode;
  transferManifest: TransferManifestTelemetry[];
  showAuthorityEntities: boolean;
  showGnsConnections: boolean;
  hoveredShardId: string | null;
  showEntityOwnershipHover: boolean;
  filteredShardIdSet: Set<string> | null;
}

interface MapDerivedData {
  combinedShapes: ShapeJS[];
  hoveredShardEdgeLabels: HoveredShardEdgeLabel[];
  networkEdgeCount: number;
  networkNodeIds: string[];
  networkNodeIdSet: Set<string>;
  shardHoverBoundsById: Map<string, ShardHoverBounds>;
  shardHoverRegionsById: Map<string, ShardHoverRegion[]>;
  shardPolygonsById: Map<string, { x: number; y: number }[][]>;
  shardTelemetryById: Map<string, ShardTelemetry>;
}

export function useMapDerivedData({
  authorityEntities,
  authorityLinkMode,
  baseShapes,
  filteredShardIdSet,
  hoveredShardId,
  networkTelemetry,
  showEntityOwnershipHover,
  transferManifest,
  showAuthorityEntities,
  showGnsConnections,
}: UseMapDerivedDataArgs): MapDerivedData {
  const ownerPositions = useMemo(
    () => computeOwnerPositions(authorityEntities),
    [authorityEntities]
  );

  const filteredBaseShapes = useMemo(
    () =>
      buildFilteredBaseShapes({
        baseShapes,
        filteredShardIdSet,
      }),
    [baseShapes, filteredShardIdSet]
  );

  const shardAnchorPositions = useMemo(
    () => computeShardAnchorPositions(baseShapes),
    [baseShapes]
  );

  const mapBoundsCenter = useMemo(
    () => computeMapBoundsCenter(baseShapes),
    [baseShapes]
  );

  const { shardBoundsById, shardPolygonsById, shardRegionsById } = useMemo(
    () => computeShardGeometryById({ baseShapes }),
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
        authorityLinkMode,
        networkTelemetry,
        networkNodeIds,
        networkNodeIdSet,
        ownerPositions,
        projectedShardPositions,
        transferManifest,
        showAuthorityEntities,
        showGnsConnections,
        hoveredShardId,
        showEntityOwnershipHover,
        filteredShardIdSet,
      }),
    [
      authorityEntities,
      authorityLinkMode,
      networkTelemetry,
      networkNodeIds,
      networkNodeIdSet,
      ownerPositions,
      projectedShardPositions,
      transferManifest,
      showAuthorityEntities,
      showGnsConnections,
      hoveredShardId,
      showEntityOwnershipHover,
      filteredShardIdSet,
    ]
  );

  const combinedShapes = useMemo(
    () => [...filteredBaseShapes, ...overlayShapes],
    [filteredBaseShapes, overlayShapes]
  );

  return {
    combinedShapes,
    hoveredShardEdgeLabels,
    networkEdgeCount,
    networkNodeIds,
    networkNodeIdSet,
    shardHoverBoundsById,
    shardHoverRegionsById: shardRegionsById,
    shardPolygonsById,
    shardTelemetryById,
  };
}
