import { NextResponse } from 'next/server';
import type {
  DatabaseRecord,
  DatabaseSnapshotResponse,
  DatabaseSource,
} from '../../lib/cartographTypes';
import { normalizeDatabaseSnapshot } from '../../lib/server/databaseSnapshot';
import { fetchNativeJson } from '../../lib/server/nativeClient';

const TIMEOUT_MS = 1500;
const MAX_MATCHED_RECORDS = 300;

type MatchField = 'key' | 'payload' | 'both';

interface MatchTerm {
  label: string;
  value: string;
  weight: number;
}

interface MatchResult {
  matchedIn: MatchField;
  matchedTerms: string[];
  relevance: number;
}

function parseOptionalBooleanFlag(raw: string): boolean | undefined {
  const normalized = raw.trim().toLowerCase();
  if (normalized.length === 0) {
    return undefined;
  }
  if (
    normalized === '1' ||
    normalized === 'true' ||
    normalized === 'on' ||
    normalized === 'yes'
  ) {
    return true;
  }
  if (
    normalized === '0' ||
    normalized === 'false' ||
    normalized === 'off' ||
    normalized === 'no'
  ) {
    return false;
  }
  return undefined;
}

function addTerm(
  terms: MatchTerm[],
  seen: Set<string>,
  label: string,
  raw: string | null,
  weight: number
): void {
  const value = (raw ?? '').trim();
  if (!value) {
    return;
  }
  const dedupeKey = `${label}:${value.toLowerCase()}`;
  if (seen.has(dedupeKey)) {
    return;
  }
  seen.add(dedupeKey);
  terms.push({ label, value, weight });
}

function buildMatchTerms(args: {
  entityId: string;
  ownerId: string;
  clientId: string;
  world: string;
}): MatchTerm[] {
  const terms: MatchTerm[] = [];
  const seen = new Set<string>();

  addTerm(terms, seen, 'entityId', args.entityId, 12);
  addTerm(terms, seen, 'entityTag', `entity=${args.entityId}`, 10);

  addTerm(terms, seen, 'ownerId', args.ownerId, 8);
  addTerm(terms, seen, 'ownerTag', `owner=${args.ownerId}`, 6);

  addTerm(terms, seen, 'clientId', args.clientId, 5);
  addTerm(terms, seen, 'clientTag', `client=${args.clientId}`, 4);

  if (args.world.trim().length > 0) {
    addTerm(terms, seen, 'worldTag', `world=${args.world.trim()}`, 3);
  }

  return terms;
}

function evaluateRecordMatch(record: DatabaseRecord, terms: MatchTerm[]): MatchResult | null {
  const keyLower = record.key.toLowerCase();
  const payloadLower = record.payload.toLowerCase();

  let keyMatched = false;
  let payloadMatched = false;
  let relevance = 0;
  const matchedTerms = new Set<string>();

  for (const term of terms) {
    const normalized = term.value.toLowerCase();
    let matchedThisTerm = false;

    if (normalized.length > 0 && keyLower.includes(normalized)) {
      keyMatched = true;
      relevance += term.weight + 4;
      matchedThisTerm = true;
    }
    if (normalized.length > 0 && payloadLower.includes(normalized)) {
      payloadMatched = true;
      relevance += term.weight;
      matchedThisTerm = true;
    }

    if (matchedThisTerm) {
      matchedTerms.add(term.value);
    }
  }

  if (!keyMatched && !payloadMatched) {
    return null;
  }

  let matchedIn: MatchField = 'payload';
  if (keyMatched && payloadMatched) {
    matchedIn = 'both';
  } else if (keyMatched) {
    matchedIn = 'key';
  }

  return {
    matchedIn,
    matchedTerms: Array.from(matchedTerms.values()),
    relevance,
  };
}

function sortMatches(
  left: DatabaseRecord & MatchResult,
  right: DatabaseRecord & MatchResult
): number {
  if (left.relevance !== right.relevance) {
    return right.relevance - left.relevance;
  }
  const sourceCompare = left.source.localeCompare(right.source);
  if (sourceCompare !== 0) {
    return sourceCompare;
  }
  return left.key.localeCompare(right.key);
}

function resolveSources(
  sources: DatabaseSource[],
  requestedSource: string
): DatabaseSource[] {
  if (!requestedSource) {
    return sources;
  }

  const requestedLower = requestedSource.toLowerCase();
  return sources.filter(
    (source) =>
      source.id.toLowerCase() === requestedLower ||
      source.name.toLowerCase() === requestedLower
  );
}

export async function GET(req: Request) {
  const reqUrl = new URL(req.url);
  const entityId = (reqUrl.searchParams.get('entityId') ?? '').trim();
  const ownerId = (reqUrl.searchParams.get('ownerId') ?? '').trim();
  const clientId = (reqUrl.searchParams.get('clientId') ?? '').trim();
  const world = (reqUrl.searchParams.get('world') ?? '').trim();
  const requestedSource = (reqUrl.searchParams.get('source') ?? '').trim();

  if (!entityId) {
    return NextResponse.json(
      { error: 'entityId query param is required.' },
      { status: 400 }
    );
  }

  const decodeSerialized =
    parseOptionalBooleanFlag(reqUrl.searchParams.get('decodeSerialized') ?? '') ??
    true;

  const sourceProbeRaw = await fetchNativeJson<unknown>({
    path: '/databases',
    timeoutMs: TIMEOUT_MS,
    query: { decodeSerialized },
  });
  const sourceProbe = normalizeDatabaseSnapshot(sourceProbeRaw);
  const matchedSources = resolveSources(sourceProbe.sources, requestedSource);
  const snapshotCache = new Map<string, DatabaseSnapshotResponse>();
  if (
    sourceProbe.selectedSource &&
    matchedSources.some((source) => source.id === sourceProbe.selectedSource)
  ) {
    snapshotCache.set(sourceProbe.selectedSource, sourceProbe);
  }

  const terms = buildMatchTerms({
    entityId,
    ownerId,
    clientId,
    world,
  });

  const matchedRecords: Array<DatabaseRecord & MatchResult> = [];
  const matchCountBySource = new Map<string, number>();
  let truncated = false;

  for (const source of matchedSources) {
    let snapshot = snapshotCache.get(source.id);
    if (!snapshot) {
      const snapshotRaw = await fetchNativeJson<unknown>({
        path: '/databases',
        timeoutMs: TIMEOUT_MS,
        query: {
          source: source.id,
          decodeSerialized,
        },
      });
      snapshot = normalizeDatabaseSnapshot(snapshotRaw);
      snapshotCache.set(source.id, snapshot);
    }

    for (const record of snapshot.records) {
      const match = evaluateRecordMatch(record, terms);
      if (!match) {
        continue;
      }
      matchedRecords.push({
        ...record,
        ...match,
      });
      matchCountBySource.set(
        source.id,
        (matchCountBySource.get(source.id) ?? 0) + 1
      );
      if (matchedRecords.length >= MAX_MATCHED_RECORDS) {
        truncated = true;
        break;
      }
    }

    if (truncated) {
      break;
    }
  }

  matchedRecords.sort(sortMatches);

  return NextResponse.json(
    {
      entity: {
        entityId,
        ownerId,
        clientId,
        world: Number(world) || 0,
      },
      searchedAt: new Date().toISOString(),
      terms: terms.map((term) => term.value),
      truncated,
      totalMatches: matchedRecords.length,
      sourceSummaries: matchedSources.map((source) => ({
        id: source.id,
        name: source.name,
        matchCount: matchCountBySource.get(source.id) ?? 0,
      })),
      records: matchedRecords,
    },
    { status: 200 }
  );
}
