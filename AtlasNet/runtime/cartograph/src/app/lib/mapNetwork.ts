import type { ShardTelemetry } from './cartographTypes';
import {
  formatRate,
  isShardIdentity,
  normalizeShardId,
  undirectedEdgeKey,
  type Point2,
} from './mapDataCore';

export interface HoveredShardEdgeLabel {
  targetId: string;
  from: Point2;
  to: Point2;
  text: string;
}

export function computeNetworkNodeIds(networkTelemetry: ShardTelemetry[]): string[] {
  const ids = new Set<string>();
  for (const shard of networkTelemetry) {
    const shardId = normalizeShardId(shard.shardId);
    if (shardId.length > 0 && isShardIdentity(shardId)) {
      ids.add(shardId);
    }
    for (const connection of shard.connections) {
      const targetId = normalizeShardId(connection.targetId);
      if (targetId.length > 0 && isShardIdentity(targetId)) {
        ids.add(targetId);
      }
    }
  }

  return Array.from(ids.values()).sort();
}

export function computeNetworkEdgeCount(args: {
  networkTelemetry: ShardTelemetry[];
  networkNodeIdSet: Set<string>;
}): number {
  const { networkTelemetry, networkNodeIdSet } = args;
  const seen = new Set<string>();

  for (const shard of networkTelemetry) {
    const fromId = normalizeShardId(shard.shardId);
    if (!networkNodeIdSet.has(fromId)) {
      continue;
    }

    for (const connection of shard.connections) {
      const toId = normalizeShardId(connection.targetId);
      if (!networkNodeIdSet.has(toId)) {
        continue;
      }
      seen.add(undirectedEdgeKey(fromId, toId));
    }
  }

  return seen.size;
}

export function buildShardTelemetryById(
  networkTelemetry: ShardTelemetry[]
): Map<string, ShardTelemetry> {
  const out = new Map<string, ShardTelemetry>();
  for (const shard of networkTelemetry) {
    const shardId = normalizeShardId(shard.shardId);
    if (!isShardIdentity(shardId)) {
      continue;
    }
    out.set(shardId, shard);
  }
  return out;
}

export function buildHoveredShardEdgeLabels(args: {
  hoveredShardId: string | null;
  networkNodeIdSet: Set<string>;
  projectedShardPositions: Map<string, Point2>;
  shardTelemetryById: Map<string, ShardTelemetry>;
}): HoveredShardEdgeLabel[] {
  const {
    hoveredShardId,
    networkNodeIdSet,
    projectedShardPositions,
    shardTelemetryById,
  } = args;

  if (!hoveredShardId) {
    return [];
  }

  const shard = shardTelemetryById.get(hoveredShardId);
  const fromPos = projectedShardPositions.get(hoveredShardId);
  if (!shard || !fromPos) {
    return [];
  }

  const edgeRates = new Map<string, { inBytesPerSec: number; outBytesPerSec: number }>();
  for (const connection of shard.connections) {
    const targetId = normalizeShardId(connection.targetId);
    if (!networkNodeIdSet.has(targetId)) {
      continue;
    }

    const existing = edgeRates.get(targetId) ?? {
      inBytesPerSec: 0,
      outBytesPerSec: 0,
    };

    existing.inBytesPerSec += Number.isFinite(connection.inBytesPerSec)
      ? connection.inBytesPerSec
      : 0;
    existing.outBytesPerSec += Number.isFinite(connection.outBytesPerSec)
      ? connection.outBytesPerSec
      : 0;

    edgeRates.set(targetId, existing);
  }

  return Array.from(edgeRates.entries())
    .map(([targetId, rates]) => {
      const toPos = projectedShardPositions.get(targetId);
      if (!toPos) {
        return null;
      }

      return {
        targetId,
        from: fromPos,
        to: toPos,
        text: `In ${formatRate(rates.inBytesPerSec)} B/s • Out ${formatRate(
          rates.outBytesPerSec
        )} B/s`,
      };
    })
    .filter((value): value is HoveredShardEdgeLabel => value != null)
    .sort((a, b) => a.targetId.localeCompare(b.targetId));
}
