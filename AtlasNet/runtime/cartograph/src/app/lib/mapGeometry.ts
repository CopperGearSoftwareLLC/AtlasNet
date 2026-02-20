import type { AuthorityEntityTelemetry, ShapeJS } from './cartographTypes';
import {
  getShapeAnchorPoints,
  isShardIdentity,
  normalizeShardId,
  stableOffsetFromId,
  type Point2,
  type ShardHoverBounds,
} from './mapDataCore';

const SHARD_BOUNDS_PADDING = 2;
const SHARD_BOUNDS_FALLBACK_HALF_SIZE = 8;

export function computeOwnerPositions(
  authorityEntities: AuthorityEntityTelemetry[]
): Map<string, Point2> {
  const acc = new Map<string, { sumX: number; sumY: number; count: number }>();
  for (const entity of authorityEntities) {
    const ownerId = normalizeShardId(entity.ownerId);
    const current = acc.get(ownerId) ?? { sumX: 0, sumY: 0, count: 0 };
    current.sumX += entity.x;
    current.sumY += entity.y;
    current.count += 1;
    acc.set(ownerId, current);
  }

  const out = new Map<string, Point2>();
  for (const [ownerId, value] of acc) {
    out.set(ownerId, {
      x: value.sumX / Math.max(1, value.count),
      y: value.sumY / Math.max(1, value.count),
    });
  }
  return out;
}

export function computeShardAnchorPositions(baseShapes: ShapeJS[]): Map<string, Point2> {
  const accumulator = new Map<string, { sumX: number; sumY: number; count: number }>();
  for (const shape of baseShapes) {
    const ownerId = normalizeShardId(shape.ownerId ?? '');
    if (!isShardIdentity(ownerId)) {
      continue;
    }
    const current = accumulator.get(ownerId) ?? { sumX: 0, sumY: 0, count: 0 };
    const points = getShapeAnchorPoints(shape);
    for (const point of points) {
      current.sumX += point.x;
      current.sumY += point.y;
      current.count += 1;
    }
    accumulator.set(ownerId, current);
  }

  const out = new Map<string, Point2>();
  for (const [shardId, value] of accumulator) {
    if (value.count <= 0) {
      continue;
    }
    out.set(shardId, {
      x: value.sumX / value.count,
      y: value.sumY / value.count,
    });
  }

  return out;
}

export function computeMapBoundsCenter(baseShapes: ShapeJS[]): Point2 {
  let minX = Infinity;
  let maxX = -Infinity;
  let minY = Infinity;
  let maxY = -Infinity;
  let hasPoint = false;

  for (const shape of baseShapes) {
    const points = getShapeAnchorPoints(shape);
    for (const point of points) {
      if (!Number.isFinite(point.x) || !Number.isFinite(point.y)) {
        continue;
      }
      hasPoint = true;
      if (point.x < minX) minX = point.x;
      if (point.x > maxX) maxX = point.x;
      if (point.y < minY) minY = point.y;
      if (point.y > maxY) maxY = point.y;
    }
  }

  if (!hasPoint) {
    return { x: 0, y: 0 };
  }

  return {
    x: (minX + maxX) / 2,
    y: (minY + maxY) / 2,
  };
}

export function computeShardBoundsById(baseShapes: ShapeJS[]): Map<string, ShardHoverBounds> {
  const raw = new Map<
    string,
    { minX: number; maxX: number; minY: number; maxY: number; hasPoint: boolean }
  >();

  for (const shape of baseShapes) {
    const ownerId = normalizeShardId(shape.ownerId ?? '');
    if (!isShardIdentity(ownerId)) {
      continue;
    }

    const points = getShapeAnchorPoints(shape);
    if (points.length === 0) {
      continue;
    }

    const current = raw.get(ownerId) ?? {
      minX: Infinity,
      maxX: -Infinity,
      minY: Infinity,
      maxY: -Infinity,
      hasPoint: false,
    };

    for (const point of points) {
      if (!Number.isFinite(point.x) || !Number.isFinite(point.y)) {
        continue;
      }
      current.hasPoint = true;
      if (point.x < current.minX) current.minX = point.x;
      if (point.x > current.maxX) current.maxX = point.x;
      if (point.y < current.minY) current.minY = point.y;
      if (point.y > current.maxY) current.maxY = point.y;
    }

    raw.set(ownerId, current);
  }

  const out = new Map<string, ShardHoverBounds>();
  for (const [shardId, bounds] of raw) {
    if (!bounds.hasPoint) {
      continue;
    }
    const minX = bounds.minX - SHARD_BOUNDS_PADDING;
    const maxX = bounds.maxX + SHARD_BOUNDS_PADDING;
    const minY = bounds.minY - SHARD_BOUNDS_PADDING;
    const maxY = bounds.maxY + SHARD_BOUNDS_PADDING;
    out.set(shardId, {
      minX,
      maxX,
      minY,
      maxY,
      area: Math.max(1, (maxX - minX) * (maxY - minY)),
    });
  }

  return out;
}

export function computeProjectedShardPositions(args: {
  networkNodeIds: string[];
  shardAnchorPositions: Map<string, Point2>;
  mapBoundsCenter: Point2;
}): Map<string, Point2> {
  const { mapBoundsCenter, networkNodeIds, shardAnchorPositions } = args;
  const out = new Map<string, Point2>();

  for (const shardId of networkNodeIds) {
    const pos = shardAnchorPositions.get(shardId);
    if (pos) {
      out.set(shardId, pos);
    }
  }

  const unresolvedIds = networkNodeIds.filter((id: string) => !out.has(id)).sort();

  if (unresolvedIds.length === 0) {
    return out;
  }

  let centerX = 0;
  let centerY = 0;
  let centerCount = 0;
  for (const pos of out.values()) {
    centerX += pos.x;
    centerY += pos.y;
    centerCount += 1;
  }
  if (centerCount > 0) {
    centerX /= centerCount;
    centerY /= centerCount;
  } else {
    centerX = mapBoundsCenter.x;
    centerY = mapBoundsCenter.y;
  }

  for (const unresolvedId of unresolvedIds) {
    const offset = stableOffsetFromId(unresolvedId);
    out.set(unresolvedId, {
      x: centerX + offset.x,
      y: centerY + offset.y,
    });
  }

  return out;
}

export function computeShardHoverBoundsById(args: {
  networkNodeIds: string[];
  projectedShardPositions: Map<string, Point2>;
  shardBoundsById: Map<string, ShardHoverBounds>;
}): Map<string, ShardHoverBounds> {
  const { networkNodeIds, projectedShardPositions, shardBoundsById } = args;
  const out = new Map<string, ShardHoverBounds>(shardBoundsById);

  for (const shardId of networkNodeIds) {
    if (out.has(shardId)) {
      continue;
    }

    const anchor = projectedShardPositions.get(shardId);
    if (!anchor) {
      continue;
    }

    const minX = anchor.x - SHARD_BOUNDS_FALLBACK_HALF_SIZE;
    const maxX = anchor.x + SHARD_BOUNDS_FALLBACK_HALF_SIZE;
    const minY = anchor.y - SHARD_BOUNDS_FALLBACK_HALF_SIZE;
    const maxY = anchor.y + SHARD_BOUNDS_FALLBACK_HALF_SIZE;
    out.set(shardId, {
      minX,
      maxX,
      minY,
      maxY,
      area: (maxX - minX) * (maxY - minY),
    });
  }

  return out;
}
