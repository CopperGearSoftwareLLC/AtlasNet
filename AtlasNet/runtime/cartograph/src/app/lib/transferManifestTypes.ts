import type {
  TransferLinkState,
  TransferManifestStage,
  TransferManifestTelemetry,
} from './cartographTypes';

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
  const values = Array.isArray(raw)
    ? raw
    : raw && typeof raw === 'object'
    ? Object.values(raw as Record<string, unknown>)
    : [];

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

function fallbackTransferId(
  fromId: string,
  toId: string,
  stage: TransferManifestStage
): string {
  return `${fromId}->${toId}:${stage}`;
}

export function parseTransferManifestRows(raw: unknown): TransferManifestTelemetry[] {
  if (!Array.isArray(raw)) {
    return [];
  }

  const rows: TransferManifestTelemetry[] = [];
  for (const item of raw) {
    if (!item || typeof item !== 'object') {
      continue;
    }

    const obj = item as Record<string, unknown>;
    const stage = normalizeStage(obj.stage ?? obj.Stage);
    const fallbackState = deriveStateFromStage(stage);
    const state = normalizeState(obj.state ?? obj.linkState ?? obj.State, fallbackState);
    const fromId = normalizeString(obj.fromId ?? obj.from ?? obj.From);
    const toId = normalizeString(obj.toId ?? obj.to ?? obj.To);
    const entityIds = parseEntityIds(obj.entityIds ?? obj.EntityIDs ?? obj.entityIDs);

    if (!fromId || !toId || entityIds.length === 0) {
      continue;
    }

    const transferId =
      normalizeString(obj.transferId ?? obj.id ?? obj.TransferID) ||
      fallbackTransferId(fromId, toId, stage);

    rows.push({
      transferId,
      fromId,
      toId,
      stage,
      state,
      entityIds,
    });
  }

  return rows;
}
