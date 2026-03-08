import type {
  AuthorityLinkMode,
  AuthorityEntityTelemetry,
  ShapeJS,
  ShardTelemetry,
  TransferManifestTelemetry,
} from './cartographTypes';
import { normalizeShardId, undirectedEdgeKey, type Point2 } from './mapDataCore';

const TRANSFER_STAGE_COLOR_BY_NAME: Record<string, string> = {
  none: 'rgba(148, 163, 184, 0.9)',
  prepare: 'rgba(251, 191, 36, 0.92)',
  ready: 'rgba(56, 189, 248, 0.92)',
  commit: 'rgba(249, 115, 22, 0.92)',
  complete: 'rgba(34, 197, 94, 0.92)',
  unknown: 'rgba(148, 163, 184, 0.9)',
};

interface HandoffEntityLink {
  fromId: string;
  toId: string;
  stage: string;
  state: string;
}
const NON_HOVERED_ENTITY_DIM_FACTOR = 0.22;

type TransferStageName =
  | 'none'
  | 'prepare'
  | 'ready'
  | 'commit'
  | 'complete'
  | 'unknown';

function normalizeEnumText(rawValue: string): string {
  const trimmed = rawValue.trim();
  if (!trimmed) {
    return '';
  }

  const withoutEnumPrefix = trimmed.replace(/^e(?=[A-Z])/, '');
  const withSpaces = withoutEnumPrefix
    .replace(/[_-]+/g, ' ')
    .replace(/([a-z0-9])([A-Z])/g, '$1 $2')
    .trim();
  return withSpaces.toLowerCase();
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

function resolveTransferStageColor(stage: string): string {
  return TRANSFER_STAGE_COLOR_BY_NAME[resolveTransferStageName(stage)];
}

function formatTransferStageLabel(stage: string): string {
  return resolveTransferStageName(stage);
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

function resolveHandoffTargetShardId(link: HandoffEntityLink): string {
  const normalizedState = normalizeEnumText(link.state);
  if (normalizedState === 'target') {
    return link.toId;
  }
  if (normalizedState === 'source') {
    return link.fromId;
  }
  if (resolveTransferStageName(link.stage) === 'complete') {
    return link.toId;
  }
  return link.fromId;
}

function resolveHandoffLineColor(link: HandoffEntityLink): string {
  return resolveTransferStageColor(link.stage);
}

function resolveHandoffLineLabel(link: HandoffEntityLink): string {
  return formatTransferStageLabel(link.stage);
}

function scaleColorAlpha(color: string, factor: number): string {
  if (factor >= 1) {
    return color;
  }
  const match = color.match(
    /^rgba?\(\s*([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)(?:\s*,\s*([0-9.]+))?\s*\)$/i
  );
  if (!match) {
    return color;
  }

  const r = Number(match[1]);
  const g = Number(match[2]);
  const b = Number(match[3]);
  const a = match[4] == null ? 1 : Number(match[4]);
  if (
    !Number.isFinite(r) ||
    !Number.isFinite(g) ||
    !Number.isFinite(b) ||
    !Number.isFinite(a)
  ) {
    return color;
  }

  const alpha = Math.max(0, Math.min(1, a * factor));
  return `rgba(${Math.round(r)}, ${Math.round(g)}, ${Math.round(b)}, ${alpha})`;
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
        resolveHandoffLineColor(handoffLink),
        dimmed ? NON_HOVERED_ENTITY_DIM_FACTOR : 1
      ),
      label: resolveHandoffLineLabel(handoffLink),
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

  const isClient = Boolean(entity.isClient);
  return {
    type: 'line',
    position: { x: 0, y: 0 },
    points: [
      { x: entity.x, y: entity.y },
      { x: ownerPos.x, y: ownerPos.y },
    ],
    color: scaleColorAlpha(
      isClient ? 'rgba(255, 96, 96, 0.9)' : 'rgba(255, 255, 0, 0.9)',
      dimmed ? NON_HOVERED_ENTITY_DIM_FACTOR : 1
    ),
  };
}

function buildAuthorityEntityNodeOverlay(
  entity: AuthorityEntityTelemetry,
  dimmed: boolean
): ShapeJS {
  const isClient = Boolean(entity.isClient);
  return {
    type: 'circle',
    position: { x: entity.x, y: entity.y },
    radius: 1.8,
    color: scaleColorAlpha(
      isClient ? 'rgba(255, 72, 72, 1)' : 'rgba(255, 240, 80, 1)',
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
  showEntityOwnershipHover: boolean
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
    const dimmed =
      hoveredShardIdNormalized.length > 0 &&
      ownerIdNormalized !== hoveredShardIdNormalized;
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
        args.showEntityOwnershipHover
      )
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
