import type { HalfPlane2, ShapeJS, Vec2 } from '../../shared/cartographTypes';

export interface Point2 {
  x: number;
  y: number;
}

export interface PointBounds2 {
  minX: number;
  maxX: number;
  minY: number;
  maxY: number;
}

function asFiniteNumber(value: unknown): number | null {
  const numeric = Number(value);
  return Number.isFinite(numeric) ? numeric : null;
}

export function isFinitePoint(point: Point2 | null | undefined): point is Point2 {
  return Boolean(point && Number.isFinite(point.x) && Number.isFinite(point.y));
}

export function normalizePoint(value: unknown): Point2 | null {
  if (!value || typeof value !== 'object') {
    return null;
  }
  const x = asFiniteNumber((value as { x?: unknown }).x);
  const y = asFiniteNumber((value as { y?: unknown }).y);
  if (x == null || y == null) {
    return null;
  }
  return { x, y };
}

export function normalizeHalfPlanes(value: unknown): HalfPlane2[] {
  if (typeof value === 'string') {
    try {
      return normalizeHalfPlanes(JSON.parse(value));
    } catch {
      return [];
    }
  }

  if (!Array.isArray(value)) {
    return [];
  }

  const out: HalfPlane2[] = [];
  for (const plane of value) {
    if (!plane || typeof plane !== 'object') {
      continue;
    }

    const planeObject = plane as {
      nx?: unknown;
      ny?: unknown;
      c?: unknown;
      normal?: { x?: unknown; y?: unknown };
      n?: { x?: unknown; y?: unknown };
      offset?: unknown;
      constant?: unknown;
    };

    const nx =
      asFiniteNumber(planeObject.nx) ??
      asFiniteNumber(planeObject.normal?.x) ??
      asFiniteNumber(planeObject.n?.x);
    const ny =
      asFiniteNumber(planeObject.ny) ??
      asFiniteNumber(planeObject.normal?.y) ??
      asFiniteNumber(planeObject.n?.y);
    const c =
      asFiniteNumber(planeObject.c) ??
      asFiniteNumber(planeObject.offset) ??
      asFiniteNumber(planeObject.constant);

    if (nx == null || ny == null || c == null) {
      continue;
    }

    out.push({ nx, ny, c });
  }

  return out;
}

export function computePolygonSignedArea(points: Point2[]): number {
  if (points.length < 3) {
    return 0;
  }

  let area2 = 0;
  for (let i = 0; i < points.length; i += 1) {
    const a = points[i];
    const b = points[(i + 1) % points.length];
    area2 += a.x * b.y - b.x * a.y;
  }
  return area2 * 0.5;
}

export function computePointBounds(points: Point2[]): PointBounds2 | null {
  let minX = Infinity;
  let maxX = -Infinity;
  let minY = Infinity;
  let maxY = -Infinity;
  let hasPoint = false;

  for (const point of points) {
    if (!isFinitePoint(point)) {
      continue;
    }
    hasPoint = true;
    if (point.x < minX) minX = point.x;
    if (point.x > maxX) maxX = point.x;
    if (point.y < minY) minY = point.y;
    if (point.y > maxY) maxY = point.y;
  }

  if (!hasPoint) {
    return null;
  }

  return { minX, maxX, minY, maxY };
}

export function expandBounds(bounds: PointBounds2, padding: number): PointBounds2 {
  return {
    minX: bounds.minX - padding,
    maxX: bounds.maxX + padding,
    minY: bounds.minY - padding,
    maxY: bounds.maxY + padding,
  };
}

export function boundsToPolygon(bounds: PointBounds2): Point2[] {
  return [
    { x: bounds.minX, y: bounds.minY },
    { x: bounds.maxX, y: bounds.minY },
    { x: bounds.maxX, y: bounds.maxY },
    { x: bounds.minX, y: bounds.maxY },
  ];
}

function intersectSegmentWithHalfPlane(
  start: Point2,
  end: Point2,
  plane: HalfPlane2
): Point2 {
  const dx = end.x - start.x;
  const dy = end.y - start.y;
  const denom = plane.nx * dx + plane.ny * dy;
  if (Math.abs(denom) < 1e-8) {
    return start;
  }

  const startEval = plane.nx * start.x + plane.ny * start.y;
  const t = (plane.c - startEval) / denom;
  const clampedT = Math.max(0, Math.min(1, t));
  return {
    x: start.x + dx * clampedT,
    y: start.y + dy * clampedT,
  };
}

export function clipPolygonWithHalfPlane(
  polygon: Point2[],
  plane: HalfPlane2
): Point2[] {
  if (polygon.length === 0) {
    return [];
  }

  const out: Point2[] = [];
  const inside = (point: Point2) =>
    plane.nx * point.x + plane.ny * point.y <= plane.c + 1e-5;

  for (let i = 0; i < polygon.length; i += 1) {
    const current = polygon[i];
    const previous = polygon[(i + polygon.length - 1) % polygon.length];
    const currentInside = inside(current);
    const previousInside = inside(previous);

    if (currentInside) {
      if (!previousInside) {
        out.push(intersectSegmentWithHalfPlane(previous, current, plane));
      }
      out.push(current);
    } else if (previousInside) {
      out.push(intersectSegmentWithHalfPlane(previous, current, plane));
    }
  }

  return out;
}

function getClipEdgeHalfPlane(
  start: Point2,
  end: Point2,
  isClockwise: boolean
): HalfPlane2 {
  const dx = end.x - start.x;
  const dy = end.y - start.y;
  const nx = isClockwise ? -dy : dy;
  const ny = isClockwise ? dx : -dx;
  return {
    nx,
    ny,
    c: nx * start.x + ny * start.y,
  };
}

export function clipPolygonToConvexPolygon(
  polygon: Point2[],
  clipPolygon: Point2[]
): Point2[] {
  if (polygon.length < 3 || clipPolygon.length < 3) {
    return [];
  }

  let out = polygon.slice();
  const clipArea = computePolygonSignedArea(clipPolygon);
  const isClockwise = clipArea < 0;

  for (let i = 0; i < clipPolygon.length; i += 1) {
    const start = clipPolygon[i];
    const end = clipPolygon[(i + 1) % clipPolygon.length];
    out = clipPolygonWithHalfPlane(out, getClipEdgeHalfPlane(start, end, isClockwise));
    if (out.length < 3) {
      return [];
    }
  }

  return out;
}

export function clipHalfPlaneCellToPolygon(
  clipPolygon: Point2[],
  halfPlanes: HalfPlane2[]
): Point2[] {
  if (clipPolygon.length < 3 || halfPlanes.length === 0) {
    return [];
  }

  let out = clipPolygon.slice();
  for (const plane of halfPlanes) {
    out = clipPolygonWithHalfPlane(out, plane);
    if (out.length < 3) {
      return [];
    }
  }
  return out;
}

export function pointWithinHalfPlanes(point: Point2, halfPlanes: HalfPlane2[]): boolean {
  if (halfPlanes.length === 0) {
    return false;
  }

  for (const plane of halfPlanes) {
    if (plane.nx * point.x + plane.ny * point.y > plane.c + 1e-5) {
      return false;
    }
  }
  return true;
}

export function shapeHasHalfPlaneCell(shape: ShapeJS): boolean {
  return Array.isArray(shape.halfPlanes) && shape.halfPlanes.length > 0;
}

export function getShapeSite(shape: ShapeJS): Point2 | null {
  const explicitSite = normalizePoint(shape.site);
  if (explicitSite) {
    return explicitSite;
  }

  return normalizePoint(shape.position);
}

export function getFiniteWorldPolygon(shape: ShapeJS): Point2[] {
  if (shape.type !== 'polygon' || !Array.isArray(shape.points) || shape.points.length < 3) {
    return [];
  }

  // Finite polygons in Cartograph use `points` as local offsets from `position`.
  // Half-plane cells still use `site` separately when no finite polygon exists.
  const base = normalizePoint(shape.position) ?? getShapeSite(shape);
  if (!base) {
    return [];
  }

  const out: Point2[] = [];
  for (const point of shape.points) {
    if (!isFinitePoint(point)) {
      continue;
    }
    out.push({ x: base.x + point.x, y: base.y + point.y });
  }
  return out.length >= 3 ? out : [];
}

export function resolveShapeWorldPolygon(
  shape: ShapeJS,
  clipPolygon?: Point2[] | null
): Point2[] {
  const finitePolygon = getFiniteWorldPolygon(shape);
  if (finitePolygon.length >= 3) {
    if (clipPolygon && clipPolygon.length >= 3) {
      return clipPolygonToConvexPolygon(finitePolygon, clipPolygon);
    }
    return finitePolygon;
  }

  if (!shapeHasHalfPlaneCell(shape) || !clipPolygon || clipPolygon.length < 3) {
    return [];
  }

  return clipHalfPlaneCellToPolygon(clipPolygon, shape.halfPlanes ?? []);
}

export function collectShapeAnchorPoints(shape: ShapeJS): Point2[] {
  const center = getShapeSite(shape);

  if (shape.type === 'polygon') {
    const polygon = getFiniteWorldPolygon(shape);
    if (polygon.length > 0) {
      return polygon;
    }
    if (center) {
      return [center];
    }
    return [];
  }

  if (shape.type === 'line' && Array.isArray(shape.points) && shape.points.length > 0) {
    const out: Point2[] = [];
    for (const point of shape.points) {
      if (isFinitePoint(point)) {
        out.push({ x: point.x, y: point.y });
      }
    }
    return out;
  }

  if (!center) {
    return [];
  }

  if (shape.type === 'rectangle' || shape.type === 'rectImage') {
    const halfX = (shape.size?.x ?? 0) / 2;
    const halfY = (shape.size?.y ?? 0) / 2;
    return [
      { x: center.x - halfX, y: center.y - halfY },
      { x: center.x + halfX, y: center.y - halfY },
      { x: center.x + halfX, y: center.y + halfY },
      { x: center.x - halfX, y: center.y + halfY },
    ];
  }

  if (shape.type === 'circle') {
    const radius = Math.abs(shape.radius ?? 0);
    if (radius > 0) {
      return [
        { x: center.x - radius, y: center.y },
        { x: center.x + radius, y: center.y },
        { x: center.x, y: center.y - radius },
        { x: center.x, y: center.y + radius },
      ];
    }
  }

  return [center];
}

export function buildFramePolygonFromPoints(
  points: Point2[],
  padding: number
): Point2[] {
  const bounds = computePointBounds(points);
  if (!bounds) {
    return [];
  }
  return boundsToPolygon(expandBounds(bounds, padding));
}

export function toLocalPolygon(points: Point2[], origin: Vec2): Vec2[] {
  return points.map((point) => ({
    x: point.x - origin.x,
    y: point.y - origin.y,
  }));
}
