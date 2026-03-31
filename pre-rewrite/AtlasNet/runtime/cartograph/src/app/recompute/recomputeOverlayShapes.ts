import type {
  HotspotInput,
  RecomputeSnapshotTelemetry,
  ShapeJS,
} from '../shared/cartographTypes';

export interface RecomputeOverlayLayer {
  id: string;
  title: string;
  subtitle: string;
  shapeCount: number;
  shapes: ShapeJS[];
  rawSource: 'input' | 'output';
}

const WORLD_MIN_X = -100;
const WORLD_MAX_X = 100;
const WORLD_MIN_Y = -100;
const WORLD_MAX_Y = 100;
const WORLD_WIDTH = WORLD_MAX_X - WORLD_MIN_X;
const WORLD_HEIGHT = WORLD_MAX_Y - WORLD_MIN_Y;

function clamp01(value: number): number {
  return Math.min(1, Math.max(0, value));
}

function hasFinitePoint(value: unknown): value is { x?: unknown; y?: unknown } {
  return !!value && typeof value === 'object';
}

function toWorldCoordinate(value: number, min: number, size: number): number {
  if (!Number.isFinite(value)) {
    return min;
  }
  if (value >= 0 && value <= 1) {
    return min + clamp01(value) * size;
  }
  return value;
}

function toWorldPoint(point: { x?: unknown; y?: unknown }): { x: number; y: number } | null {
  const x = Number(point.x);
  const y = Number(point.y);
  if (!Number.isFinite(x) || !Number.isFinite(y)) {
    return null;
  }
  return {
    x: toWorldCoordinate(x, WORLD_MIN_X, WORLD_WIDTH),
    y: toWorldCoordinate(y, WORLD_MIN_Y, WORLD_HEIGHT),
  };
}

function hotspotToShape(
  hotspot: HotspotInput,
  snapshotId: number,
  index: number
): ShapeJS {
  return {
    id: `hotspot-${snapshotId}-${index}`,
    type: 'circle',
    position: {
      x: WORLD_MIN_X + clamp01(hotspot.x) * WORLD_WIDTH,
      y: WORLD_MIN_Y + clamp01(hotspot.y) * WORLD_HEIGHT,
    },
    radius: Math.max(
      1.5,
      clamp01(hotspot.radius) * Math.max(WORLD_WIDTH, WORLD_HEIGHT)
    ),
    color: 'rgba(56, 189, 248, 0.95)',
  };
}

function pointToShape(
  point: { x?: unknown; y?: unknown },
  id: string,
  color: string
): ShapeJS | null {
  const worldPoint = toWorldPoint(point);
  if (!worldPoint) {
    return null;
  }
  return {
    id,
    type: 'circle',
    position: worldPoint,
    radius: 2.2,
    color,
  };
}

function polygonToShape(
  verticesRaw: unknown,
  id: string,
  color: string
): ShapeJS | null {
  if (!Array.isArray(verticesRaw) || verticesRaw.length < 3) {
    return null;
  }

  const vertices = verticesRaw
    .filter(hasFinitePoint)
    .map((point) => toWorldPoint(point))
    .filter((point): point is { x: number; y: number } => point != null);

  if (vertices.length < 3) {
    return null;
  }

  const centroid = vertices.reduce(
    (acc, point) => ({
      x: acc.x + point.x,
      y: acc.y + point.y,
    }),
    { x: 0, y: 0 }
  );
  centroid.x /= vertices.length;
  centroid.y /= vertices.length;

  return {
    id,
    type: 'polygon',
    position: centroid,
    points: vertices.map((point) => ({
      x: point.x - centroid.x,
      y: point.y - centroid.y,
    })),
    color,
  };
}

function halfPlaneCellToShape(
  raw: Record<string, unknown>,
  id: string,
  color: string
): ShapeJS | null {
  const siteRaw = raw.site;
  if (!siteRaw || typeof siteRaw !== 'object' || Array.isArray(siteRaw)) {
    return null;
  }

  const siteX = Number((siteRaw as { x?: unknown }).x);
  const siteY = Number((siteRaw as { y?: unknown }).y);
  if (!Number.isFinite(siteX) || !Number.isFinite(siteY)) {
    return null;
  }

  const halfPlanesRaw = Array.isArray(raw.halfPlanes) ? raw.halfPlanes : [];
  const halfPlanes = halfPlanesRaw
    .filter((plane): plane is Record<string, unknown> => !!plane && typeof plane === 'object')
    .map((plane) => ({
      nx: Number(plane.nx),
      ny: Number(plane.ny),
      c: Number(plane.c),
    }))
    .filter(
      (plane) =>
        Number.isFinite(plane.nx) &&
        Number.isFinite(plane.ny) &&
        Number.isFinite(plane.c)
    );

  if (halfPlanes.length === 0) {
    return null;
  }

  return {
    id,
    type: 'polygon',
    position: { x: siteX, y: siteY },
    points: [],
    site: { x: siteX, y: siteY },
    halfPlanes,
    color,
  };
}

function rectangleToShape(
  raw: Record<string, unknown>,
  id: string,
  color: string
): ShapeJS | null {
  const minX = Number(raw.min_x ?? raw.minX);
  const maxX = Number(raw.max_x ?? raw.maxX);
  const minY = Number(raw.min_y ?? raw.minY);
  const maxY = Number(raw.max_y ?? raw.maxY);

  if (
    !Number.isFinite(minX) ||
    !Number.isFinite(maxX) ||
    !Number.isFinite(minY) ||
    !Number.isFinite(maxY)
  ) {
    return null;
  }

  const worldMinX = toWorldCoordinate(minX, WORLD_MIN_X, WORLD_WIDTH);
  const worldMaxX = toWorldCoordinate(maxX, WORLD_MIN_X, WORLD_WIDTH);
  const worldMinY = toWorldCoordinate(minY, WORLD_MIN_Y, WORLD_HEIGHT);
  const worldMaxY = toWorldCoordinate(maxY, WORLD_MIN_Y, WORLD_HEIGHT);

  return {
    id,
    type: 'rectangle',
    position: {
      x: (worldMinX + worldMaxX) / 2,
      y: (worldMinY + worldMaxY) / 2,
    },
    size: {
      x: Math.abs(worldMaxX - worldMinX),
      y: Math.abs(worldMaxY - worldMinY),
    },
    color,
  };
}

function extractHotspotLayer(
  snapshot: RecomputeSnapshotTelemetry
): RecomputeOverlayLayer | null {
  const hotspots = Array.isArray(snapshot.inputJson?.hotspots)
    ? snapshot.inputJson.hotspots
    : [];
  if (hotspots.length === 0) {
    return null;
  }

  return {
    id: `${snapshot.snapshotId}:hotspots`,
    title: 'Hotspots',
    subtitle: snapshot.recomputeHeuristic,
    shapeCount: hotspots.length,
    rawSource: 'input',
    shapes: hotspots.map((hotspot, index) =>
      hotspotToShape(hotspot, snapshot.snapshotId, index)
    ),
  };
}

function extractPointLayer(
  snapshot: RecomputeSnapshotTelemetry,
  pointsRaw: unknown,
  kind: 'input-points' | 'input-seeds' | 'output-seeds',
  title: string,
  color: string,
  rawSource: 'input' | 'output'
): RecomputeOverlayLayer | null {
  if (!Array.isArray(pointsRaw)) {
    return null;
  }

  const shapes = pointsRaw
    .filter(hasFinitePoint)
    .map((point, index) =>
      pointToShape(point, `${snapshot.snapshotId}:${kind}:${index}`, color)
    )
    .filter((shape): shape is ShapeJS => shape != null);

  if (shapes.length === 0) {
    return null;
  }

  return {
    id: `${snapshot.snapshotId}:${kind}`,
    title,
    subtitle: snapshot.recomputeHeuristic,
    shapeCount: shapes.length,
    rawSource,
    shapes,
  };
}

function extractCellLayer(
  snapshot: RecomputeSnapshotTelemetry,
  cellsRaw: unknown
): RecomputeOverlayLayer | null {
  if (!Array.isArray(cellsRaw)) {
    return null;
  }

  const shapes: ShapeJS[] = [];
  for (let index = 0; index < cellsRaw.length; index += 1) {
    const cell = cellsRaw[index];
    if (!cell || typeof cell !== 'object' || Array.isArray(cell)) {
      continue;
    }

    const polygon = polygonToShape(
      (cell as Record<string, unknown>).vertices,
      `${snapshot.snapshotId}:cell-poly:${index}`,
      'rgba(248, 113, 113, 0.92)'
    );
    if (polygon) {
      shapes.push(polygon);
      continue;
    }

    const halfPlaneCell = halfPlaneCellToShape(
      cell as Record<string, unknown>,
      `${snapshot.snapshotId}:cell-halfplane:${index}`,
      'rgba(248, 113, 113, 0.92)'
    );
    if (halfPlaneCell) {
      shapes.push(halfPlaneCell);
      continue;
    }

    const rectangle = rectangleToShape(
      cell as Record<string, unknown>,
      `${snapshot.snapshotId}:cell-rect:${index}`,
      'rgba(248, 113, 113, 0.92)'
    );
    if (rectangle) {
      shapes.push(rectangle);
    }
  }

  if (shapes.length === 0) {
    return null;
  }

  return {
    id: `${snapshot.snapshotId}:cells`,
    title: 'Cells',
    subtitle: snapshot.recomputeHeuristic,
    shapeCount: shapes.length,
    rawSource: 'output',
    shapes,
  };
}

function extractBoundsLayer(
  snapshot: RecomputeSnapshotTelemetry,
  boundsRaw: unknown
): RecomputeOverlayLayer | null {
  if (!Array.isArray(boundsRaw)) {
    return null;
  }

  const shapes = boundsRaw
    .filter((bound): bound is Record<string, unknown> => !!bound && typeof bound === 'object')
    .map((bound, index) =>
      rectangleToShape(
        bound,
        `${snapshot.snapshotId}:bound:${index}`,
        'rgba(34, 197, 94, 0.92)'
      )
    )
    .filter((shape): shape is ShapeJS => shape != null);

  if (shapes.length === 0) {
    return null;
  }

  return {
    id: `${snapshot.snapshotId}:bounds`,
    title: 'Bounds',
    subtitle: snapshot.recomputeHeuristic,
    shapeCount: shapes.length,
    rawSource: 'output',
    shapes,
  };
}

export function buildRecomputeOverlayLayers(
  snapshots: RecomputeSnapshotTelemetry[]
): RecomputeOverlayLayer[] {
  const layers: RecomputeOverlayLayer[] = [];

  for (const snapshot of snapshots) {
    const hotspotLayer = extractHotspotLayer(snapshot);
    if (hotspotLayer) {
      layers.push(hotspotLayer);
    }

    const inputPoints = extractPointLayer(
      snapshot,
      snapshot.inputJsonRaw?.points,
      'input-points',
      'Input Points',
      'rgba(251, 191, 36, 0.98)',
      'input'
    );
    if (inputPoints) {
      layers.push(inputPoints);
    }

    const inputSeeds = extractPointLayer(
      snapshot,
      snapshot.inputJsonRaw?.seeds,
      'input-seeds',
      'Input Seeds',
      'rgba(251, 191, 36, 0.98)',
      'input'
    );
    if (inputSeeds) {
      layers.push(inputSeeds);
    }

    const outputSeeds = extractPointLayer(
      snapshot,
      snapshot.outputJsonRaw?.seeds,
      'output-seeds',
      'Output Seeds',
      'rgba(248, 113, 113, 0.98)',
      'output'
    );
    if (outputSeeds) {
      layers.push(outputSeeds);
    }

    const cellsLayer = extractCellLayer(snapshot, snapshot.outputJsonRaw?.cells);
    if (cellsLayer) {
      layers.push(cellsLayer);
    }

    const boundsLayer = extractBoundsLayer(snapshot, snapshot.outputJsonRaw?.bounds);
    if (boundsLayer) {
      layers.push(boundsLayer);
    }
  }

  return layers;
}
