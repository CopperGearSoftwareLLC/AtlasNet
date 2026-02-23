import type {
  DatabaseRecord,
  DatabaseSnapshotResponse,
  DatabaseSource,
} from '../cartographTypes';

const EMPTY_SNAPSHOT: DatabaseSnapshotResponse = {
  sources: [],
  selectedSource: null,
  records: [],
};

function asObject(value: unknown): Record<string, unknown> | null {
  return value && typeof value === 'object' ? (value as Record<string, unknown>) : null;
}

function asFiniteNumber(value: unknown, fallback = 0): number {
  const n = Number(value);
  return Number.isFinite(n) ? n : fallback;
}

function asNullableFiniteNumber(value: unknown): number | null {
  if (value == null) {
    return null;
  }
  const n = Number(value);
  return Number.isFinite(n) ? n : null;
}

function parseDatabaseSource(raw: unknown): DatabaseSource | null {
  const source = asObject(raw);
  if (!source) {
    return null;
  }

  const id = String(source.id ?? '').trim();
  const name = String(source.name ?? '').trim();
  const host = String(source.host ?? '').trim();
  if (!id || !name || !host) {
    return null;
  }

  return {
    id,
    name,
    host,
    port: asFiniteNumber(source.port),
    running: Boolean(source.running),
    latencyMs: asNullableFiniteNumber(source.latencyMs),
  };
}

function parseDatabaseRecord(raw: unknown): DatabaseRecord | null {
  const record = asObject(raw);
  if (!record) {
    return null;
  }

  const key = String(record.key ?? '').trim();
  const source = String(record.source ?? '').trim();
  if (!key || !source) {
    return null;
  }

  return {
    source,
    key,
    type: String(record.type ?? 'unknown'),
    entryCount: asFiniteNumber(record.entryCount),
    ttlSeconds: asFiniteNumber(record.ttlSeconds, -2),
    payload: String(record.payload ?? ''),
  };
}

export function normalizeDatabaseSnapshot(raw: unknown): DatabaseSnapshotResponse {
  const payload = asObject(raw);
  if (!payload) {
    return EMPTY_SNAPSHOT;
  }

  const sources = Array.isArray(payload.sources)
    ? payload.sources
        .map(parseDatabaseSource)
        .filter((source): source is DatabaseSource => source != null)
    : [];
  const records = Array.isArray(payload.records)
    ? payload.records
        .map(parseDatabaseRecord)
        .filter((record): record is DatabaseRecord => record != null)
    : [];

  const selectedSourceRaw = payload.selectedSource;
  const selectedSource =
    selectedSourceRaw == null ? null : String(selectedSourceRaw).trim() || null;

  return {
    sources,
    selectedSource,
    records,
  };
}
