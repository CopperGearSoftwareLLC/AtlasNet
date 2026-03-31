import type {
  AuthorityEntityTelemetry,
  TransferLinkState,
  TransferManifestStage,
  TransferManifestTelemetry,
  TransferStateQueueTelemetry,
} from './cartographTypes';

const EMPTY_AUTHORITY_ROWS: AuthorityEntityTelemetry[] = [];
const EMPTY_TRANSFER_MANIFEST_ROWS: TransferManifestTelemetry[] = [];
const EMPTY_TRANSFER_QUEUE_ROWS: TransferStateQueueTelemetry[] = [];

const VALID_TRANSFER_STAGES: ReadonlySet<TransferManifestStage> = new Set([
  'eNone',
  'ePrepare',
  'eReady',
  'eCommit',
  'eComplete',
  'eUnknown',
]);

const VALID_TRANSFER_STATES: ReadonlySet<TransferLinkState> = new Set([
  'source',
  'target',
]);

function toNumber(value: unknown, fallback = 0): number {
  const n = Number(value);
  return Number.isFinite(n) ? n : fallback;
}

function normalizeString(value: unknown): string {
  if (typeof value !== 'string') {
    return '';
  }
  return value.trim();
}

function normalizeStage(value: unknown): TransferManifestStage {
  const raw = normalizeString(value);
  if (VALID_TRANSFER_STAGES.has(raw as TransferManifestStage)) {
    return raw as TransferManifestStage;
  }
  return 'eUnknown';
}

function deriveStateFromStage(stage: TransferManifestStage): TransferLinkState {
  switch (stage) {
    case 'eReady':
    case 'eCommit':
    case 'eComplete':
      return 'target';
    case 'eNone':
    case 'ePrepare':
    case 'eUnknown':
    default:
      return 'source';
  }
}

function normalizeState(
  value: unknown,
  fallback: TransferLinkState
): TransferLinkState {
  const raw = normalizeString(value);
  if (VALID_TRANSFER_STATES.has(raw as TransferLinkState)) {
    return raw as TransferLinkState;
  }
  return fallback;
}

function parseEntityIds(raw: unknown): string[] {
  const values = Array.isArray(raw) ? raw : [];

  const out: string[] = [];
  const seen = new Set<string>();
  for (const value of values) {
    const id = normalizeString(value);
    if (!id || seen.has(id)) {
      continue;
    }
    seen.add(id);
    out.push(id);
  }
  return out;
}

function normalizeTimestampMsOptional(value: unknown): number | undefined {
  const parsed = Number(value);
  if (!Number.isFinite(parsed) || parsed <= 0) {
    return undefined;
  }
  return Math.floor(parsed);
}

function normalizeTimestampMsRequired(value: unknown): number {
  const parsed = Number(value);
  if (!Number.isFinite(parsed) || parsed <= 0) {
    return Date.now();
  }
  return Math.floor(parsed);
}

function parseTransferRows<T extends TransferManifestTelemetry | TransferStateQueueTelemetry>(
  raw: unknown,
  normalizeTimestamp: (value: unknown) => T['timestampMs'],
  emptyRows: T[]
): T[] {
  if (!Array.isArray(raw)) {
    return emptyRows;
  }

  const rows: T[] = [];
  for (const item of raw) {
    if (!item || typeof item !== 'object') {
      continue;
    }

    const obj = item as Record<string, unknown>;
    const stage = normalizeStage(obj.stage);
    const state = normalizeState(obj.state, deriveStateFromStage(stage));
    const fromId = normalizeString(obj.fromId);
    const toId = normalizeString(obj.toId);
    const entityIds = parseEntityIds(obj.entityIds);
    if (!fromId || !toId || entityIds.length === 0) {
      continue;
    }

    rows.push({
      transferId: normalizeString(obj.transferId) || `${fromId}->${toId}:${stage}`,
      fromId,
      toId,
      stage,
      state,
      entityIds,
      timestampMs: normalizeTimestamp(obj.timestampMs),
    } as T);
  }

  return rows.length > 0 ? rows : emptyRows;
}

// Authority telemetry is emitted as tuple rows:
// [entityId, ownerId, world, x, y, z, isClient, clientId]
export function parseAuthorityRows(raw: unknown): AuthorityEntityTelemetry[] {
  if (!Array.isArray(raw)) {
    return EMPTY_AUTHORITY_ROWS;
  }

  const rows: AuthorityEntityTelemetry[] = [];
  for (const item of raw) {
    if (!Array.isArray(item) || item.length < 8) {
      continue;
    }
    rows.push({
      entityId: String(item[0]),
      ownerId: String(item[1]),
      world: toNumber(item[2]),
      x: toNumber(item[3]),
      y: toNumber(item[4]),
      z: toNumber(item[5]),
      isClient: String(item[6]) === '1' || item[6] === true,
      clientId: String(item[7]),
    });
  }

  const normalized = rows.filter(
    (row) => row.entityId.length > 0 && row.ownerId.length > 0
  );
  return normalized.length > 0 ? normalized : EMPTY_AUTHORITY_ROWS;
}

export function parseTransferManifestRows(raw: unknown): TransferManifestTelemetry[] {
  return parseTransferRows<TransferManifestTelemetry>(
    raw,
    normalizeTimestampMsOptional,
    EMPTY_TRANSFER_MANIFEST_ROWS
  );
}

export function parseTransferStateQueueRows(raw: unknown): TransferStateQueueTelemetry[] {
  return parseTransferRows<TransferStateQueueTelemetry>(
    raw,
    normalizeTimestampMsRequired,
    EMPTY_TRANSFER_QUEUE_ROWS
  );
}
