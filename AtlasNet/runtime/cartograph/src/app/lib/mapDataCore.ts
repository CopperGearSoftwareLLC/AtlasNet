import type { ShapeJS } from './cartographTypes';

export interface Point2 {
  x: number;
  y: number;
}

export interface ShardHoverBounds {
  minX: number;
  maxX: number;
  minY: number;
  maxY: number;
  area: number;
}

export function formatRate(value: number): string {
  if (!Number.isFinite(value)) {
    return '0.0';
  }
  return value.toFixed(1);
}

export function normalizeShardId(value: string): string {
  return value.trim();
}

export function isShardIdentity(value: string): boolean {
  return normalizeShardId(value).startsWith('eShard ');
}

export function stableOffsetFromId(id: string): Point2 {
  let hash = 2166136261;
  for (let i = 0; i < id.length; i += 1) {
    hash ^= id.charCodeAt(i);
    hash = Math.imul(hash, 16777619);
  }
  const unsigned = hash >>> 0;
  const angle = ((unsigned % 360) * Math.PI) / 180;
  const radius = 22 + (unsigned % 17);
  return { x: Math.cos(angle) * radius, y: Math.sin(angle) * radius };
}

export function getShapeAnchorPoints(shape: ShapeJS): Point2[] {
  const cx = shape.position.x ?? 0;
  const cy = shape.position.y ?? 0;

  if ((shape.type === 'polygon' || shape.type === 'line') && shape.points) {
    if (shape.points.length > 0) {
      return shape.points.map((point) => ({ x: point.x, y: point.y }));
    }
  }

  if (shape.type === 'rectangle' || shape.type === 'rectImage') {
    const halfX = (shape.size?.x ?? 0) / 2;
    const halfY = (shape.size?.y ?? 0) / 2;
    return [
      { x: cx - halfX, y: cy - halfY },
      { x: cx + halfX, y: cy - halfY },
      { x: cx + halfX, y: cy + halfY },
      { x: cx - halfX, y: cy + halfY },
    ];
  }

  if (shape.type === 'circle') {
    const radius = Math.abs(shape.radius ?? 0);
    if (radius > 0) {
      return [
        { x: cx - radius, y: cy },
        { x: cx + radius, y: cy },
        { x: cx, y: cy - radius },
        { x: cx, y: cy + radius },
      ];
    }
  }

  return [{ x: cx, y: cy }];
}

export function undirectedEdgeKey(left: string, right: string): string {
  return left < right ? `${left}|${right}` : `${right}|${left}`;
}
