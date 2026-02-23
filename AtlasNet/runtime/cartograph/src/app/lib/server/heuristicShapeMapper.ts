import type { ShapeJS, Vec2 } from '../cartographTypes';

interface RawShape {
  id?: unknown;
  ownerId?: unknown;
  type?: unknown;
  position?: {
    x?: unknown;
    y?: unknown;
  };
  radius?: unknown;
  size?: {
    x?: unknown;
    y?: unknown;
  };
  vertices?: unknown;
  points?: unknown;
  color?: unknown;
}

function asNumber(value: unknown, fallback = 0): number {
  const n = Number(value);
  return Number.isFinite(n) ? n : fallback;
}

function toPoints(points: unknown): Vec2[] {
  if (!Array.isArray(points)) {
    return [];
  }

  const result: Vec2[] = [];
  for (const point of points) {
    if (point && typeof point === 'object') {
      const x = asNumber((point as { x?: unknown }).x, NaN);
      const y = asNumber((point as { y?: unknown }).y, NaN);
      if (Number.isFinite(x) && Number.isFinite(y)) {
        result.push({ x, y });
        continue;
      }
    }

    if (Array.isArray(point) && point.length >= 2) {
      const x = asNumber(point[0], NaN);
      const y = asNumber(point[1], NaN);
      if (Number.isFinite(x) && Number.isFinite(y)) {
        result.push({ x, y });
      }
    }
  }

  return result;
}

function convertShape(rawShape: RawShape): ShapeJS {
  const shape: ShapeJS = {
    id: rawShape.id == null ? undefined : String(rawShape.id),
    ownerId: rawShape.ownerId == null ? undefined : String(rawShape.ownerId),
    type: 'circle',
    position: {
      x: asNumber(rawShape.position?.x),
      y: asNumber(rawShape.position?.y),
    },
    color: rawShape.color == null ? undefined : String(rawShape.color),
  };

  switch (asNumber(rawShape.type, -1)) {
    case 0:
      shape.type = 'circle';
      shape.radius = Math.abs(asNumber(rawShape.radius, 10));
      break;
    case 1:
      shape.type = 'rectangle';
      shape.size = {
        x: asNumber(rawShape.size?.x, 10),
        y: asNumber(rawShape.size?.y, 10),
      };
      break;
    case 2:
      shape.type = 'line';
      shape.points = toPoints(rawShape.vertices ?? rawShape.points);
      break;
    case 3:
      shape.type = 'polygon';
      shape.points = toPoints(rawShape.vertices ?? rawShape.points);
      break;
    case 4:
      shape.type = 'rectImage';
      shape.size = {
        x: asNumber(rawShape.size?.x, 10),
        y: asNumber(rawShape.size?.y, 10),
      };
      break;
    default:
      break;
  }

  return shape;
}

export function normalizeHeuristicShapes(raw: unknown): ShapeJS[] {
  if (!Array.isArray(raw)) {
    return [];
  }

  return raw
    .filter((item): item is RawShape => item != null && typeof item === 'object')
    .map(convertShape);
}
