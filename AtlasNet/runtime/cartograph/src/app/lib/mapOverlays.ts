import type {
  AuthorityLinkMode,
  AuthorityEntityTelemetry,
  ShapeJS,
  ShardTelemetry,
  TransferManifestStage,
  TransferManifestTelemetry,
} from './cartographTypes';
import { normalizeShardId, undirectedEdgeKey, type Point2 } from './mapDataCore';

const TRANSFER_STAGE_COLORS: Record<TransferManifestStage, string> = {
  eNone: 'rgba(148, 163, 184, 0.9)',
  ePrepare: 'rgba(251, 191, 36, 0.92)',
  eReady: 'rgba(56, 189, 248, 0.92)',
  eCommit: 'rgba(249, 115, 22, 0.92)',
  eComplete: 'rgba(34, 197, 94, 0.92)',
  eUnknown: 'rgba(148, 163, 184, 0.9)',
};

interface HandoffEntityLink {
  fromId: string;
  toId: string;
  stage: TransferManifestStage;
  state: 'source' | 'target';
}

function transferStageRank(stage: TransferManifestStage): number {
  switch (stage) {
    case 'eNone':
      return 0;
    case 'ePrepare':
      return 1;
    case 'eReady':
      return 2;
    case 'eCommit':
      return 3;
    case 'eComplete':
      return 4;
    case 'eUnknown':
    default:
      return -1;
  }
}

function indexHandoffLinksByEntity(
  transferManifest: TransferManifestTelemetry[]
): Map<string, HandoffEntityLink> {
  const byEntity = new Map<string, HandoffEntityLink>();

  for (const transfer of transferManifest) {
    const fromId = normalizeShardId(transfer.fromId);
    const toId = normalizeShardId(transfer.toId);
    if (!fromId || !toId) {
      continue;
    }

    const candidate: HandoffEntityLink = {
      fromId,
      toId,
      stage: transfer.stage,
      state: transfer.state,
    };
    const candidateRank = transferStageRank(candidate.stage);
    for (const rawEntityId of transfer.entityIds) {
      const entityId = String(rawEntityId).trim();
      if (!entityId) {
        continue;
      }

      const current = byEntity.get(entityId);
      if (!current || transferStageRank(current.stage) <= candidateRank) {
        byEntity.set(entityId, candidate);
      }
    }
  }

  return byEntity;
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
}): ShapeJS[] {
  const overlays: ShapeJS[] = [];

  if (args.showAuthorityEntities) {
    overlays.push(
      ...buildAuthorityEntityOverlays(
        args.authorityEntities,
        args.authorityLinkMode,
        args.projectedShardPositions,
        args.transferManifest
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

function buildAuthorityEntityOverlays(
  authorityEntities: AuthorityEntityTelemetry[],
  authorityLinkMode: AuthorityLinkMode,
  projectedShardPositions: Map<string, Point2>,
  transferManifest: TransferManifestTelemetry[]
): ShapeJS[] {
  const overlays: ShapeJS[] = [];
  const handoffLinksByEntity =
    authorityLinkMode === 'handoff'
      ? indexHandoffLinksByEntity(transferManifest)
      : null;

  for (const entity of authorityEntities) {
    const isClient = Boolean(entity.isClient);
    overlays.push({
      type: 'circle',
      position: { x: entity.x, y: entity.y },
      radius: 1.8,
      color: isClient ? 'rgba(255, 72, 72, 1)' : 'rgba(255, 240, 80, 1)',
    });

    if (authorityLinkMode === 'owner') {
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
        color: isClient ? 'rgba(255, 96, 96, 0.9)' : 'rgba(255, 255, 0, 0.9)',
      });
      continue;
    }

    const handoffLink = handoffLinksByEntity?.get(String(entity.entityId).trim());
    if (!handoffLink) {
      continue;
    }

    const linkTargetShardId =
      handoffLink.state === 'target' ? handoffLink.toId : handoffLink.fromId;
    const linkTarget = projectedShardPositions.get(normalizeShardId(linkTargetShardId));
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
      color: TRANSFER_STAGE_COLORS[handoffLink.stage],
      label: `state: ${handoffLink.state}`,
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
