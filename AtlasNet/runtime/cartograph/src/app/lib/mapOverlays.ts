import type {
  AuthorityEntityTelemetry,
  ShapeJS,
  ShardTelemetry,
} from './cartographTypes';
import { normalizeShardId, undirectedEdgeKey, type Point2 } from './mapDataCore';

export function buildOverlayShapes(args: {
  authorityEntities: AuthorityEntityTelemetry[];
  networkTelemetry: ShardTelemetry[];
  networkNodeIds: string[];
  networkNodeIdSet: Set<string>;
  ownerPositions: Map<string, Point2>;
  projectedShardPositions: Map<string, Point2>;
  showAuthorityEntities: boolean;
  showGnsConnections: boolean;
}): ShapeJS[] {
  const overlays: ShapeJS[] = [];

  if (args.showAuthorityEntities) {
    overlays.push(
      ...buildAuthorityEntityOverlays(args.authorityEntities, args.projectedShardPositions)
    );
  }

  if (args.showGnsConnections) {
    overlays.push(
      ...buildGnsConnectionOverlays(
        args.networkTelemetry,
        args.networkNodeIdSet,
        args.projectedShardPositions
      ),
      ...buildShardNodeOverlays(
        args.networkNodeIds,
        args.ownerPositions,
        args.projectedShardPositions
      )
    );
  }

  return overlays;
}

function buildAuthorityEntityOverlays(
  authorityEntities: AuthorityEntityTelemetry[],
  projectedShardPositions: Map<string, Point2>
): ShapeJS[] {
  const overlays: ShapeJS[] = [];
  for (const entity of authorityEntities) {
    overlays.push({
      type: 'circle',
      position: { x: entity.x, y: entity.y },
      radius: 1.8,
      color: 'rgba(255, 240, 80, 1)',
    });

    const ownerPos = projectedShardPositions.get(normalizeShardId(entity.ownerId));
    if (!ownerPos) {
      continue;
    }

    overlays.push({
      type: 'line',
      position: { x: 0, y: 0 },
      points: [
        { x: entity.x, y: entity.y },
        { x: ownerPos.x, y: ownerPos.y },
      ],
      color: 'rgba(255, 255, 0, 0.9)',
    });
  }
  return overlays;
}

function buildGnsConnectionOverlays(
  networkTelemetry: ShardTelemetry[],
  networkNodeIdSet: Set<string>,
  projectedShardPositions: Map<string, Point2>
): ShapeJS[] {
  const overlays: ShapeJS[] = [];
  const seen = new Set<string>();

  for (const shard of networkTelemetry) {
    const fromId = normalizeShardId(shard.shardId);
    if (!networkNodeIdSet.has(fromId)) {
      continue;
    }

    const fromPos = projectedShardPositions.get(fromId);
    if (!fromPos) {
      continue;
    }

    for (const connection of shard.connections) {
      const toId = normalizeShardId(connection.targetId);
      if (!networkNodeIdSet.has(toId)) {
        continue;
      }

      const toPos = projectedShardPositions.get(toId);
      if (!toPos) {
        continue;
      }

      const key = undirectedEdgeKey(fromId, toId);
      if (seen.has(key)) {
        continue;
      }
      seen.add(key);

      overlays.push({
        type: 'line',
        position: { x: 0, y: 0 },
        points: [
          { x: fromPos.x, y: fromPos.y },
          { x: toPos.x, y: toPos.y },
        ],
        color: 'rgba(80, 200, 255, 0.65)',
      });
    }
  }

  return overlays;
}

function buildShardNodeOverlays(
  networkNodeIds: string[],
  ownerPositions: Map<string, Point2>,
  projectedShardPositions: Map<string, Point2>
): ShapeJS[] {
  const overlays: ShapeJS[] = [];

  for (const shardId of networkNodeIds) {
    const anchor = projectedShardPositions.get(shardId);
    if (!anchor) {
      continue;
    }

    const hasOwnerSample = ownerPositions.has(shardId);
    overlays.push({
      type: 'circle',
      position: anchor,
      radius: hasOwnerSample ? 2.6 : 2.1,
      color: hasOwnerSample
        ? 'rgba(80, 200, 255, 0.95)'
        : 'rgba(120, 170, 220, 0.75)',
    });
  }

  return overlays;
}
