import type { ShardTelemetry } from './cartographTypes';

export interface GraphNode {
  id: string;
  x: number;
  y: number;
}

export interface GraphEdge {
  from: string;
  to: string;
  inBytesPerSec: number;
  outBytesPerSec: number;
}

export interface GraphNodeStats {
  downloadKbps: number;
  uploadKbps: number;
  connections: number;
}

type ConnectionLike =
  | {
      targetId: string;
      inBytesPerSec?: number;
      outBytesPerSec?: number;
    }
  | string[];

function getConnectionTarget(connection: ConnectionLike): string | null {
  if (Array.isArray(connection)) {
    return connection.length > 1 ? String(connection[1]) : null;
  }

  return connection.targetId ?? null;
}

function getConnectionRates(
  connection: ConnectionLike
): { inBytesPerSec: number; outBytesPerSec: number } | null {
  if (Array.isArray(connection)) {
    if (connection.length < 5) {
      return null;
    }
    const inBytesPerSec = Number(connection[3]);
    const outBytesPerSec = Number(connection[4]);
    if (!Number.isFinite(inBytesPerSec) || !Number.isFinite(outBytesPerSec)) {
      return null;
    }
    return { inBytesPerSec, outBytesPerSec };
  }

  if (
    typeof connection.inBytesPerSec === 'number' &&
    typeof connection.outBytesPerSec === 'number'
  ) {
    return {
      inBytesPerSec: connection.inBytesPerSec,
      outBytesPerSec: connection.outBytesPerSec,
    };
  }

  return null;
}

export function buildNetworkGraph(telemetry: ShardTelemetry[]): {
  nodeIds: string[];
  edges: GraphEdge[];
} {
  const nodeIds = telemetry.map((shard) => shard.shardId);
  const nodeIdSet = new Set(nodeIds);
  const edgeMap = new Map<string, GraphEdge>();

  for (const shard of telemetry) {
    for (const rawConnection of shard.connections as ConnectionLike[]) {
      const targetId = getConnectionTarget(rawConnection);
      if (!targetId || !nodeIdSet.has(targetId)) {
        continue;
      }

      const sourceId = shard.shardId;
      if (sourceId === targetId) {
        continue;
      }

      const rates = getConnectionRates(rawConnection);
      const edgeKey = `${sourceId}->${targetId}`;
      const existing = edgeMap.get(edgeKey);
      const inRate = rates?.inBytesPerSec ?? 0;
      const outRate = rates?.outBytesPerSec ?? 0;

      if (existing) {
        existing.inBytesPerSec += inRate;
        existing.outBytesPerSec += outRate;
      } else {
        edgeMap.set(edgeKey, {
          from: sourceId,
          to: targetId,
          inBytesPerSec: inRate,
          outBytesPerSec: outRate,
        });
      }
    }
  }

  return {
    nodeIds,
    edges: Array.from(edgeMap.values()),
  };
}

export function buildNodeStats(
  telemetry: ShardTelemetry[]
): Map<string, GraphNodeStats> {
  const stats = new Map<string, GraphNodeStats>();
  for (const shard of telemetry) {
    stats.set(shard.shardId, {
      downloadKbps: shard.downloadKbps,
      uploadKbps: shard.uploadKbps,
      connections: shard.connections.length,
    });
  }
  return stats;
}

export function buildCircularNodes(
  nodeIds: string[],
  width: number,
  height: number
): GraphNode[] {
  const centerX = width / 2;
  const centerY = height / 2;
  const nodeCount = nodeIds.length;
  const baseRadius = Math.min(width, height) * 0.35;
  const radius = baseRadius + Math.max(0, nodeCount - 8) * 8;

  return nodeIds.map((id, index) => {
    const angle = (Math.PI * 2 * index) / Math.max(1, nodeCount);
    const x = centerX + Math.cos(angle) * radius;
    const y = centerY + Math.sin(angle) * radius;
    return { id, x, y };
  });
}
