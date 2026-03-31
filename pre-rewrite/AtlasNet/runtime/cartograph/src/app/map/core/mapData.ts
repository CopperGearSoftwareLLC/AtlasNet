import type {
  HalfPlane2,
  AuthorityLinkMode,
  AuthorityEntityTelemetry,
  ShapeJS,
  ShardTelemetry,
  TransferManifestTelemetry,
} from '../../shared/cartographTypes';
import {
  collectShapeAnchorPoints,
  computePolygonSignedArea,
  getShapeSite,
  pointWithinHalfPlanes,
  resolveShapeWorldPolygon,
  shapeHasHalfPlaneCell,
  type Point2,
} from './shapeGeometry';
export type { Point2 } from './shapeGeometry';

export interface ShardHoverBounds {
  minX: number;
  maxX: number;
  minY: number;
  maxY: number;
  area: number;
}

export interface ShardHoverRegionPolygon {
  kind: 'polygon';
  points: Point2[];
  area: number;
}

export interface ShardHoverRegionHalfPlaneCell {
  kind: 'halfPlaneCell';
  site: Point2;
  halfPlanes: HalfPlane2[];
  area: number;
}

export type ShardHoverRegion =
  | ShardHoverRegionPolygon
  | ShardHoverRegionHalfPlaneCell;

export interface HoveredShardEdgeLabel {
  targetId: string;
  from: Point2;
  to: Point2;
  text: string;
}

const SHARD_BOUNDS_PADDING = 2;
const SHARD_BOUNDS_FALLBACK_HALF_SIZE = 8;
const NON_HOVERED_ENTITY_DIM_FACTOR = 0.22;
const DEFAULT_SHAPE_COLOR = 'rgba(100, 149, 255, 1)';

const UUID_PATTERN =
  /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

const TRANSFER_STAGE_COLOR_BY_NAME: Record<string, string> = {
  none: 'rgba(148, 163, 184, 0.9)',
  prepare: 'rgba(251, 191, 36, 0.92)',
  ready: 'rgba(56, 189, 248, 0.92)',
  commit: 'rgba(249, 115, 22, 0.92)',
  complete: 'rgba(34, 197, 94, 0.92)',
  unknown: 'rgba(148, 163, 184, 0.9)',
};

type TransferStageName =
  | 'none'
  | 'prepare'
  | 'ready'
  | 'commit'
  | 'complete'
  | 'unknown';

interface HandoffEntityLink {
  fromId: string;
  toId: string;
  stage: string;
  state: string;
}

export function formatRate(value: number): string {
  if (!Number.isFinite(value)) {
    return '0.0';
  }
  return value.toFixed(1);
}

export function normalizeShardId(value: string): string {
  const text = String(value ?? '').replace(/\0/g, '').trim();
  if (!text) {
    return '';
  }
  if (text.startsWith('eShard ')) {
    return text;
  }
  if (UUID_PATTERN.test(text)) {
    return `eShard ${text}`;
  }
  return text;
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
  return collectShapeAnchorPoints(shape);
}

export function undirectedEdgeKey(left: string, right: string): string {
  return left < right ? `${left}|${right}` : `${right}|${left}`;
}

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
    for (const point of getShapeAnchorPoints(shape)) {
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
    for (const point of getShapeAnchorPoints(shape)) {
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

export function pointInPolygon(point: Point2, polygon: Point2[]): boolean {
  if (polygon.length < 3) {
    return false;
  }

  let inside = false;
  for (let i = 0, j = polygon.length - 1; i < polygon.length; j = i, i += 1) {
    const xi = polygon[i].x;
    const yi = polygon[i].y;
    const xj = polygon[j].x;
    const yj = polygon[j].y;

    const intersects =
      yi > point.y !== yj > point.y &&
      point.x < ((xj - xi) * (point.y - yi)) / (yj - yi + 1e-12) + xi;
    if (intersects) {
      inside = !inside;
    }
  }
  return inside;
}

export function pointWithinShardRegion(point: Point2, region: ShardHoverRegion): boolean {
  if (region.kind === 'polygon') {
    return pointInPolygon(point, region.points);
  }
  return pointWithinHalfPlanes(point, region.halfPlanes);
}

export function computeShardGeometryById(args: {
  baseShapes: ShapeJS[];
  clipPolygon?: Point2[] | null;
}): {
  shardBoundsById: Map<string, ShardHoverBounds>;
  shardPolygonsById: Map<string, Point2[][]>;
  shardRegionsById: Map<string, ShardHoverRegion[]>;
  claimedShardIds: Set<string>;
} {
  const { baseShapes, clipPolygon } = args;
  const rawBounds = new Map<
    string,
    { minX: number; maxX: number; minY: number; maxY: number; hasPoint: boolean }
  >();
  const shardPolygonsById = new Map<string, Point2[][]>();
  const shardRegionsById = new Map<string, ShardHoverRegion[]>();
  const claimedShardIds = new Set<string>();

  for (const shape of baseShapes) {
    const ownerId = normalizeShardId(shape.ownerId ?? '');
    if (!isShardIdentity(ownerId)) {
      continue;
    }

    claimedShardIds.add(ownerId);

    const resolvedPolygon = resolveShapeWorldPolygon(shape, clipPolygon);
    if (resolvedPolygon.length >= 3) {
      const bounds = rawBounds.get(ownerId) ?? {
        minX: Infinity,
        maxX: -Infinity,
        minY: Infinity,
        maxY: -Infinity,
        hasPoint: false,
      };

      for (const point of resolvedPolygon) {
        if (!Number.isFinite(point.x) || !Number.isFinite(point.y)) {
          continue;
        }
        bounds.hasPoint = true;
        if (point.x < bounds.minX) bounds.minX = point.x;
        if (point.x > bounds.maxX) bounds.maxX = point.x;
        if (point.y < bounds.minY) bounds.minY = point.y;
        if (point.y > bounds.maxY) bounds.maxY = point.y;
      }
      rawBounds.set(ownerId, bounds);

      const existingPolygons = shardPolygonsById.get(ownerId);
      if (existingPolygons) {
        existingPolygons.push(resolvedPolygon);
      } else {
        shardPolygonsById.set(ownerId, [resolvedPolygon]);
      }

      const polygonArea = Math.max(1, Math.abs(computePolygonSignedArea(resolvedPolygon)));
      const existingRegions = shardRegionsById.get(ownerId) ?? [];
      existingRegions.push({
        kind: 'polygon',
        points: resolvedPolygon,
        area: polygonArea,
      });
      shardRegionsById.set(ownerId, existingRegions);
      continue;
    }

    if (shapeHasHalfPlaneCell(shape)) {
      const site = getShapeSite(shape);
      if (!site) {
        continue;
      }
      const existingRegions = shardRegionsById.get(ownerId) ?? [];
      existingRegions.push({
        kind: 'halfPlaneCell',
        site,
        halfPlanes: shape.halfPlanes ?? [],
        area: Number.POSITIVE_INFINITY,
      });
      shardRegionsById.set(ownerId, existingRegions);
    }
  }

  const shardBoundsById = new Map<string, ShardHoverBounds>();
  for (const [shardId, bounds] of rawBounds) {
    if (!bounds.hasPoint) {
      continue;
    }

    const minX = bounds.minX - SHARD_BOUNDS_PADDING;
    const maxX = bounds.maxX + SHARD_BOUNDS_PADDING;
    const minY = bounds.minY - SHARD_BOUNDS_PADDING;
    const maxY = bounds.maxY + SHARD_BOUNDS_PADDING;
    shardBoundsById.set(shardId, {
      minX,
      maxX,
      minY,
      maxY,
      area: Math.max(1, (maxX - minX) * (maxY - minY)),
    });
  }

  return {
    shardBoundsById,
    shardPolygonsById,
    shardRegionsById,
    claimedShardIds,
  };
}

export function computeShardBoundsById(baseShapes: ShapeJS[]): Map<string, ShardHoverBounds> {
  return computeShardGeometryById({ baseShapes }).shardBoundsById;
}

export function computeShardPolygonsById(baseShapes: ShapeJS[]): Map<string, Point2[][]> {
  return computeShardGeometryById({ baseShapes }).shardPolygonsById;
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
      area: Math.max(1, (maxX - minX) * (maxY - minY)),
    });
  }

  return out;
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

function normalizeEnumText(rawValue: string): string {
  const trimmed = rawValue.trim();
  if (!trimmed) {
    return '';
  }

  const withoutEnumPrefix = trimmed.replace(/^e(?=[A-Z])/, '');
  return withoutEnumPrefix
    .replace(/[_-]+/g, ' ')
    .replace(/([a-z0-9])([A-Z])/g, '$1 $2')
    .trim()
    .toLowerCase();
}

function resolveTransferStageName(stage: string): TransferStageName {
  switch (normalizeEnumText(stage)) {
    case 'none':
      return 'none';
    case 'prepare':
      return 'prepare';
    case 'ready':
      return 'ready';
    case 'commit':
      return 'commit';
    case 'complete':
      return 'complete';
    default:
      return 'unknown';
  }
}

function resolveHandoffTargetShardId(link: HandoffEntityLink): string {
  const normalizedState = normalizeEnumText(link.state);
  if (normalizedState === 'target' || resolveTransferStageName(link.stage) === 'complete') {
    return link.toId;
  }
  if (normalizedState === 'source') {
    return link.fromId;
  }
  return link.fromId;
}

function scaleColorAlpha(color: string, factor: number): string {
  if (factor >= 1) {
    return color;
  }

  const value = color.trim();
  if (value.startsWith('#') && value.length === 7) {
    const r = Number.parseInt(value.slice(1, 3), 16);
    const g = Number.parseInt(value.slice(3, 5), 16);
    const b = Number.parseInt(value.slice(5, 7), 16);
    if (!Number.isFinite(r) || !Number.isFinite(g) || !Number.isFinite(b)) {
      return color;
    }
    return `rgba(${r}, ${g}, ${b}, ${Math.max(0, Math.min(1, factor))})`;
  }

  const match = value.match(
    /^rgba?\(\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)(?:\s*,\s*([0-9.]+))?\s*\)$/i
  );
  if (!match) {
    return color;
  }

  const a = match[4] == null ? 1 : Number(match[4]);
  if (!Number.isFinite(a)) {
    return color;
  }

  const alpha = Math.max(0, Math.min(1, a * factor));
  return `rgba(${Math.round(Number(match[1]))}, ${Math.round(Number(match[2]))}, ${Math.round(
    Number(match[3])
  )}, ${alpha})`;
}

function withShapeOpacity(shape: ShapeJS, factor: number): ShapeJS {
  return {
    ...shape,
    color: scaleColorAlpha(shape.color ?? DEFAULT_SHAPE_COLOR, factor),
  };
}

function isShardHighlighted(
  shardId: string,
  filteredShardIdSet: Set<string> | null
): boolean {
  if (!filteredShardIdSet || filteredShardIdSet.size === 0) {
    return true;
  }
  return !filteredShardIdSet.has(normalizeShardId(shardId));
}

export function buildFilteredBaseShapes(args: {
  baseShapes: ShapeJS[];
  filteredShardIdSet: Set<string> | null;
}): ShapeJS[] {
  const { baseShapes, filteredShardIdSet } = args;
  if (!filteredShardIdSet || filteredShardIdSet.size === 0) {
    return baseShapes;
  }

  return baseShapes.map((shape) => {
    const ownerId = normalizeShardId(shape.ownerId ?? '');
    if (!isShardIdentity(ownerId) || !filteredShardIdSet.has(ownerId)) {
      return shape;
    }
    return withShapeOpacity(shape, NON_HOVERED_ENTITY_DIM_FACTOR);
  });
}

function indexHandoffLinksByEntity(
  transferManifest: TransferManifestTelemetry[]
): Map<string, HandoffEntityLink[]> {
  const byEntity = new Map<string, HandoffEntityLink[]>();

  for (const transfer of transferManifest) {
    const fromId = normalizeShardId(transfer.fromId);
    const toId = normalizeShardId(transfer.toId);
    if (!fromId || !toId) {
      continue;
    }

    const candidate: HandoffEntityLink = {
      fromId,
      toId,
      stage: String(transfer.stage || '').trim(),
      state: String(transfer.state || '').trim(),
    };

    for (const rawEntityId of transfer.entityIds) {
      const entityId = String(rawEntityId).trim();
      if (!entityId) {
        continue;
      }
      const current = byEntity.get(entityId);
      if (current) {
        current.push(candidate);
      } else {
        byEntity.set(entityId, [candidate]);
      }
    }
  }

  return byEntity;
}

function buildHandoffEntityLinkOverlays(args: {
  entity: AuthorityEntityTelemetry;
  handoffLinks: HandoffEntityLink[];
  projectedShardPositions: Map<string, Point2>;
  dimmed: boolean;
}): ShapeJS[] {
  const { dimmed, entity, handoffLinks, projectedShardPositions } = args;
  const overlays: ShapeJS[] = [];

  for (const handoffLink of handoffLinks) {
    const targetShardId = resolveHandoffTargetShardId(handoffLink);
    const linkTarget = projectedShardPositions.get(normalizeShardId(targetShardId));
    if (!linkTarget) {
      continue;
    }

    overlays.push({
      type: 'line',
      position: { x: 0, y: 0 },
      points: [
        { x: entity.x, y: entity.y },
        { x: linkTarget.x, y: linkTarget.y },
      ],
      color: scaleColorAlpha(
        TRANSFER_STAGE_COLOR_BY_NAME[resolveTransferStageName(handoffLink.stage)],
        dimmed ? NON_HOVERED_ENTITY_DIM_FACTOR : 1
      ),
      label: resolveTransferStageName(handoffLink.stage),
    });
  }

  return overlays;
}

function buildOwnerLinkOverlay(args: {
  entity: AuthorityEntityTelemetry;
  projectedShardPositions: Map<string, Point2>;
  dimmed: boolean;
}): ShapeJS | null {
  const { dimmed, entity, projectedShardPositions } = args;
  const ownerPos = projectedShardPositions.get(normalizeShardId(entity.ownerId));
  if (!ownerPos) {
    return null;
  }

  return {
    type: 'line',
    position: { x: 0, y: 0 },
    points: [
      { x: entity.x, y: entity.y },
      { x: ownerPos.x, y: ownerPos.y },
    ],
    color: scaleColorAlpha(
      entity.isClient ? 'rgba(255, 96, 96, 0.9)' : 'rgba(255, 255, 0, 0.9)',
      dimmed ? NON_HOVERED_ENTITY_DIM_FACTOR : 1
    ),
  };
}

function buildAuthorityEntityNodeOverlay(
  entity: AuthorityEntityTelemetry,
  dimmed: boolean
): ShapeJS {
  return {
    type: 'circle',
    position: { x: entity.x, y: entity.y },
    radius: 1.8,
    color: scaleColorAlpha(
      entity.isClient ? 'rgba(255, 72, 72, 1)' : 'rgba(255, 240, 80, 1)',
      dimmed ? NON_HOVERED_ENTITY_DIM_FACTOR : 1
    ),
  };
}

function buildAuthorityEntityOverlays(
  authorityEntities: AuthorityEntityTelemetry[],
  authorityLinkMode: AuthorityLinkMode,
  projectedShardPositions: Map<string, Point2>,
  transferManifest: TransferManifestTelemetry[],
  hoveredShardId: string | null,
  showEntityOwnershipHover: boolean,
  filteredShardIdSet: Set<string> | null
): ShapeJS[] {
  const overlays: ShapeJS[] = [];
  const hoveredShardIdNormalized = showEntityOwnershipHover
    ? normalizeShardId(String(hoveredShardId ?? ''))
    : '';
  const handoffLinksByEntity =
    authorityLinkMode === 'handoff'
      ? indexHandoffLinksByEntity(transferManifest)
      : null;

  for (const entity of authorityEntities) {
    const ownerIdNormalized = normalizeShardId(entity.ownerId);
    const filteredByHover =
      hoveredShardIdNormalized.length > 0 &&
      ownerIdNormalized !== hoveredShardIdNormalized;
    const filteredByNode = !isShardHighlighted(ownerIdNormalized, filteredShardIdSet);
    const dimmed = filteredByHover || filteredByNode;
    overlays.push(buildAuthorityEntityNodeOverlay(entity, dimmed));

    if (authorityLinkMode === 'owner') {
      const ownerLine = buildOwnerLinkOverlay({
        entity,
        projectedShardPositions,
        dimmed,
      });
      if (ownerLine) {
        overlays.push(ownerLine);
      }
      continue;
    }

    const entityId = String(entity.entityId).trim();
    if (!entityId) {
      continue;
    }
    const handoffLinks = handoffLinksByEntity?.get(entityId) ?? [];
    if (handoffLinks.length === 0) {
      continue;
    }

    overlays.push(
      ...buildHandoffEntityLinkOverlays({
        entity,
        handoffLinks,
        projectedShardPositions,
        dimmed,
      })
    );
  }

  return overlays;
}

function buildGnsConnectionOverlays(
  networkTelemetry: ShardTelemetry[],
  networkNodeIdSet: Set<string>,
  projectedShardPositions: Map<string, Point2>,
  hoveredShardId: string | null,
  filteredShardIdSet: Set<string> | null
): ShapeJS[] {
  const overlays: ShapeJS[] = [];
  const seen = new Set<string>();
  const focusedShardId = normalizeShardId(String(hoveredShardId ?? ''));
  const hasFocusedShard = focusedShardId.length > 0 && networkNodeIdSet.has(focusedShardId);

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

      const isFocusedEdge =
        hasFocusedShard && (fromId === focusedShardId || toId === focusedShardId);
      const filteredByHover = hasFocusedShard && !isFocusedEdge;
      const filteredByNode =
        !isShardHighlighted(fromId, filteredShardIdSet) ||
        !isShardHighlighted(toId, filteredShardIdSet);
      const color = hasFocusedShard
        ? isFocusedEdge
          ? 'rgba(80, 200, 255, 0.9)'
          : 'rgba(80, 200, 255, 0.16)'
        : 'rgba(80, 200, 255, 0.65)';

      overlays.push({
        type: 'line',
        position: { x: 0, y: 0 },
        points: [
          { x: fromPos.x, y: fromPos.y },
          { x: toPos.x, y: toPos.y },
        ],
        color:
          filteredByNode && !filteredByHover
            ? scaleColorAlpha(color, NON_HOVERED_ENTITY_DIM_FACTOR)
            : color,
      });
    }
  }

  return overlays;
}

function computeFocusedShardNeighbors(args: {
  networkTelemetry: ShardTelemetry[];
  networkNodeIdSet: Set<string>;
  focusedShardId: string;
}): Set<string> {
  const { focusedShardId, networkNodeIdSet, networkTelemetry } = args;
  const related = new Set<string>([focusedShardId]);

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
      if (fromId === focusedShardId) {
        related.add(toId);
      }
      if (toId === focusedShardId) {
        related.add(fromId);
      }
    }
  }

  return related;
}

function buildShardNodeOverlays(
  networkNodeIds: string[],
  ownerPositions: Map<string, Point2>,
  projectedShardPositions: Map<string, Point2>,
  networkTelemetry: ShardTelemetry[],
  networkNodeIdSet: Set<string>,
  hoveredShardId: string | null,
  filteredShardIdSet: Set<string> | null
): ShapeJS[] {
  const overlays: ShapeJS[] = [];
  const focusedShardId = normalizeShardId(String(hoveredShardId ?? ''));
  const hasFocusedShard = focusedShardId.length > 0 && networkNodeIdSet.has(focusedShardId);
  const focusedRelatedIds = hasFocusedShard
    ? computeFocusedShardNeighbors({
        networkTelemetry,
        networkNodeIdSet,
        focusedShardId,
      })
    : null;

  for (const shardId of networkNodeIds) {
    const anchor = projectedShardPositions.get(shardId);
    if (!anchor) {
      continue;
    }

    const hasOwnerSample = ownerPositions.has(shardId);
    const isFocusedRelated = focusedRelatedIds?.has(shardId) ?? false;
    const filteredByHover = hasFocusedShard && !isFocusedRelated;
    const filteredByNode = !isShardHighlighted(shardId, filteredShardIdSet);
    const baseColor = hasFocusedShard
      ? isFocusedRelated
        ? hasOwnerSample
          ? 'rgba(80, 200, 255, 0.98)'
          : 'rgba(120, 170, 220, 0.9)'
        : hasOwnerSample
          ? 'rgba(80, 200, 255, 0.22)'
          : 'rgba(120, 170, 220, 0.2)'
      : hasOwnerSample
        ? 'rgba(80, 200, 255, 0.95)'
        : 'rgba(120, 170, 220, 0.75)';

    overlays.push({
      type: 'circle',
      position: anchor,
      radius: hasOwnerSample ? 2.6 : 2.1,
      color:
        filteredByNode && !filteredByHover
          ? scaleColorAlpha(baseColor, NON_HOVERED_ENTITY_DIM_FACTOR)
          : baseColor,
    });
  }

  return overlays;
}

export function buildOverlayShapes(args: {
  authorityEntities: AuthorityEntityTelemetry[];
  authorityLinkMode: AuthorityLinkMode;
  networkTelemetry: ShardTelemetry[];
  networkNodeIds: string[];
  networkNodeIdSet: Set<string>;
  ownerPositions: Map<string, Point2>;
  projectedShardPositions: Map<string, Point2>;
  transferManifest: TransferManifestTelemetry[];
  showAuthorityEntities: boolean;
  showGnsConnections: boolean;
  hoveredShardId: string | null;
  showEntityOwnershipHover: boolean;
  filteredShardIdSet: Set<string> | null;
}): ShapeJS[] {
  const overlays: ShapeJS[] = [];

  if (args.showAuthorityEntities) {
    overlays.push(
      ...buildAuthorityEntityOverlays(
        args.authorityEntities,
        args.authorityLinkMode,
        args.projectedShardPositions,
        args.transferManifest,
        args.hoveredShardId,
        args.showEntityOwnershipHover,
        args.filteredShardIdSet
      )
    );
  }

  if (args.showGnsConnections) {
    overlays.push(
      ...buildGnsConnectionOverlays(
        args.networkTelemetry,
        args.networkNodeIdSet,
        args.projectedShardPositions,
        args.hoveredShardId,
        args.filteredShardIdSet
      ),
      ...buildShardNodeOverlays(
        args.networkNodeIds,
        args.ownerPositions,
        args.projectedShardPositions,
        args.networkTelemetry,
        args.networkNodeIdSet,
        args.hoveredShardId,
        args.filteredShardIdSet
      )
    );
  }

  return overlays;
}
