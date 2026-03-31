'use client';

import { useMemo } from 'react';
import type {
  ShapeJS,
  ShardPlacementTelemetry,
  ShardTelemetry,
} from '../../shared/cartographTypes';
import {
  computeNetworkNodeIds,
  isShardIdentity,
  normalizeShardId,
} from './mapData';

export interface ClusterNodeSummary {
  nodeName: string;
  shardCount: number;
}

interface UseMapNodeFilterDataArgs {
  baseShapes: ShapeJS[];
  networkTelemetry: ShardTelemetry[];
  shardPlacement: ShardPlacementTelemetry[];
  hiddenWorkerNodeNames: string[];
}

interface MapNodeFilterData {
  clusterNodes: ClusterNodeSummary[];
  clusterNodeNames: string[];
  filteredShardIdSet: Set<string> | null;
  hiddenWorkerNodeNameSet: Set<string>;
  selectedWorkerNodeNameSet: Set<string>;
  shardWorkerNodeById: Map<string, string>;
  visibleShardCount: number;
}

function buildShardWorkerNodeLookup(
  shardPlacement: ShardPlacementTelemetry[]
): Map<string, string> {
  const out = new Map<string, string>();
  for (const row of shardPlacement) {
    const shardId = normalizeShardId(row.shardId);
    const nodeName = String(row.nodeName || '').trim();
    if (!shardId || !nodeName || out.has(shardId)) {
      continue;
    }
    out.set(shardId, nodeName);
  }
  return out;
}

function collectKnownShardIds(
  baseShapes: ShapeJS[],
  networkTelemetry: ShardTelemetry[],
  shardWorkerNodeById: Map<string, string>
): string[] {
  const out = new Set<string>();

  for (const shape of baseShapes) {
    const ownerId = normalizeShardId(shape.ownerId ?? '');
    if (isShardIdentity(ownerId)) {
      out.add(ownerId);
    }
  }

  for (const shardId of computeNetworkNodeIds(networkTelemetry)) {
    out.add(shardId);
  }

  for (const shardId of shardWorkerNodeById.keys()) {
    out.add(shardId);
  }

  return Array.from(out.values()).sort();
}

export function useMapNodeFilterData({
  baseShapes,
  hiddenWorkerNodeNames,
  networkTelemetry,
  shardPlacement,
}: UseMapNodeFilterDataArgs): MapNodeFilterData {
  const shardWorkerNodeById = useMemo(
    () => buildShardWorkerNodeLookup(shardPlacement),
    [shardPlacement]
  );

  const clusterNodes = useMemo(() => {
    const shardIdsByNodeName = new Map<string, Set<string>>();
    for (const [shardId, nodeName] of shardWorkerNodeById.entries()) {
      const existing = shardIdsByNodeName.get(nodeName);
      if (existing) {
        existing.add(shardId);
      } else {
        shardIdsByNodeName.set(nodeName, new Set([shardId]));
      }
    }

    return Array.from(shardIdsByNodeName.entries())
      .map(([nodeName, shardIds]) => ({
        nodeName,
        shardCount: shardIds.size,
      }))
      .sort((left, right) => left.nodeName.localeCompare(right.nodeName));
  }, [shardWorkerNodeById]);

  const clusterNodeNames = useMemo(
    () => clusterNodes.map((node) => node.nodeName),
    [clusterNodes]
  );

  const hiddenWorkerNodeNameSet = useMemo(
    () => new Set(hiddenWorkerNodeNames),
    [hiddenWorkerNodeNames]
  );

  const selectedWorkerNodeNameSet = useMemo(
    () =>
      new Set(
        clusterNodeNames.filter((nodeName) => !hiddenWorkerNodeNameSet.has(nodeName))
      ),
    [clusterNodeNames, hiddenWorkerNodeNameSet]
  );

  const selectedShardIdSet = useMemo(() => {
    if (hiddenWorkerNodeNames.length === 0) {
      return null;
    }

    const out = new Set<string>();
    for (const [shardId, nodeName] of shardWorkerNodeById.entries()) {
      if (selectedWorkerNodeNameSet.has(nodeName)) {
        out.add(shardId);
      }
    }
    return out;
  }, [hiddenWorkerNodeNames.length, selectedWorkerNodeNameSet, shardWorkerNodeById]);

  const allKnownShardIds = useMemo(
    () => collectKnownShardIds(baseShapes, networkTelemetry, shardWorkerNodeById),
    [baseShapes, networkTelemetry, shardWorkerNodeById]
  );

  const filteredShardIdSet = useMemo(() => {
    if (hiddenWorkerNodeNames.length === 0) {
      return null;
    }

    const visibleShardIds = selectedShardIdSet ?? new Set<string>();
    return new Set(allKnownShardIds.filter((shardId) => !visibleShardIds.has(shardId)));
  }, [allKnownShardIds, hiddenWorkerNodeNames.length, selectedShardIdSet]);

  const visibleShardCount = useMemo(() => {
    const networkNodeIds = computeNetworkNodeIds(networkTelemetry);
    if (!filteredShardIdSet) {
      return networkNodeIds.length;
    }
    return networkNodeIds.filter((shardId) => !filteredShardIdSet.has(shardId)).length;
  }, [filteredShardIdSet, networkTelemetry]);

  return {
    clusterNodes,
    clusterNodeNames,
    filteredShardIdSet,
    hiddenWorkerNodeNameSet,
    selectedWorkerNodeNameSet,
    shardWorkerNodeById,
    visibleShardCount,
  };
}
