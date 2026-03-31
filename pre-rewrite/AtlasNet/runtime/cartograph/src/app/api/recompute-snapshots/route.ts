import Redis from 'ioredis';
import { NextResponse } from 'next/server';
import type { RecomputeSnapshotsResponse } from '../../shared/cartographTypes';

const SNAPSHOT_KEY = 'Heuristic:RecomputeSnapshots';
const LEGACY_SNAPSHOT_KEY = 'Heuristic:HotspotSnapshots';
const CONNECT_TIMEOUT_MS = 700;

const EMPTY_RESPONSE: RecomputeSnapshotsResponse = {
  latestSnapshotId: 0,
  latestUpdatedMs: null,
  snapshots: [],
};

function createInternalDbClient() {
  return new Redis({
    host: process.env.INTERNAL_REDIS_SERVICE_NAME || 'internaldb',
    port: Number(process.env.INTERNAL_REDIS_PORT || 6379),
    lazyConnect: true,
    connectTimeout: CONNECT_TIMEOUT_MS,
    maxRetriesPerRequest: 0,
    enableOfflineQueue: false,
    retryStrategy: null,
  });
}

function toFiniteNumber(value: unknown, fallback = 0): number {
  const numeric = Number(value);
  return Number.isFinite(numeric) ? numeric : fallback;
}

function parsePayload(raw: unknown): RecomputeSnapshotsResponse {
  let parsed: unknown = raw;
  if (typeof raw === 'string') {
    try {
      parsed = JSON.parse(raw);
    } catch {
      parsed = null;
    }
  }

  if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) {
    return EMPTY_RESPONSE;
  }

  const payload = parsed as Record<string, unknown>;
  const snapshotsRaw = Array.isArray(payload.snapshots) ? payload.snapshots : [];

  return {
    latestSnapshotId: Math.max(
      0,
      Math.floor(toFiniteNumber(payload.latestSnapshotId, 0))
    ),
    latestUpdatedMs: Number.isFinite(toFiniteNumber(payload.latestUpdatedMs, NaN))
      ? Math.floor(toFiniteNumber(payload.latestUpdatedMs, NaN))
      : null,
    snapshots: snapshotsRaw
      .filter((entry): entry is Record<string, unknown> => !!entry && typeof entry === 'object')
      .map((entry) => ({
        snapshotId: Math.max(0, Math.floor(toFiniteNumber(entry.snapshotId, 0))),
        cycleId: Math.max(0, Math.floor(toFiniteNumber(entry.cycleId, 0))),
        createdAtMs: Math.max(0, Math.floor(toFiniteNumber(entry.createdAtMs, 0))),
        recomputeHeuristic:
          typeof entry.recomputeHeuristic === 'string' && entry.recomputeHeuristic.trim()
            ? entry.recomputeHeuristic.trim()
            : 'unknown',
        targetHeuristicType:
          typeof entry.targetHeuristicType === 'string' && entry.targetHeuristicType.trim()
            ? entry.targetHeuristicType.trim()
            : null,
        inputSchema:
          typeof entry.inputSchema === 'string' && entry.inputSchema.trim()
            ? entry.inputSchema.trim()
            : 'unknown',
        entityCount: Math.max(0, Math.floor(toFiniteNumber(entry.entityCount, 0))),
        availableServerCount: Math.max(
          0,
          Math.floor(toFiniteNumber(entry.availableServerCount, 0))
        ),
        hotspotCount: Math.max(0, Math.floor(toFiniteNumber(entry.hotspotCount, 0))),
        seedSource:
          entry.diagnostics &&
          typeof entry.diagnostics === 'object' &&
          typeof (entry.diagnostics as Record<string, unknown>).seedSource === 'string'
            ? ((entry.diagnostics as Record<string, unknown>).seedSource as string).trim() ||
              null
            : null,
        inferenceNote:
          entry.diagnostics &&
          typeof entry.diagnostics === 'object' &&
          typeof (entry.diagnostics as Record<string, unknown>).inferenceNote === 'string'
            ? ((entry.diagnostics as Record<string, unknown>).inferenceNote as string).trim() ||
              null
            : null,
        endpoint:
          entry.diagnostics &&
          typeof entry.diagnostics === 'object' &&
          typeof (entry.diagnostics as Record<string, unknown>).endpoint === 'string'
            ? ((entry.diagnostics as Record<string, unknown>).endpoint as string).trim() ||
              null
            : null,
        modelId:
          entry.diagnostics &&
          typeof entry.diagnostics === 'object' &&
          typeof (entry.diagnostics as Record<string, unknown>).modelId === 'string'
            ? ((entry.diagnostics as Record<string, unknown>).modelId as string).trim() ||
              null
            : null,
        inputJson:
          entry.input_json && typeof entry.input_json === 'object' && !Array.isArray(entry.input_json)
            ? {
                aspectRatio: toFiniteNumber(
                  (entry.input_json as Record<string, unknown>).aspect_ratio,
                  1
                ),
                k: Math.max(
                  0,
                  Math.floor(
                    toFiniteNumber((entry.input_json as Record<string, unknown>).k, 0)
                  )
                ),
                hotspots: Array.isArray(
                  (entry.input_json as Record<string, unknown>).hotspots
                )
                  ? ((entry.input_json as Record<string, unknown>).hotspots as Array<
                      Record<string, unknown>
                    >)
                      .filter((hotspot) => hotspot && typeof hotspot === 'object')
                      .map((hotspot) => ({
                        x: toFiniteNumber(hotspot.x, 0),
                        y: toFiniteNumber(hotspot.y, 0),
                        weight: toFiniteNumber(hotspot.weight, 0),
                        radius: toFiniteNumber(hotspot.radius, 0),
                      }))
                  : [],
              }
            : {},
        inputJsonRaw:
          entry.input_json && typeof entry.input_json === 'object' && !Array.isArray(entry.input_json)
            ? (entry.input_json as Record<string, unknown>)
            : {},
        outputJsonRaw:
          entry.output_json && typeof entry.output_json === 'object' && !Array.isArray(entry.output_json)
            ? (entry.output_json as Record<string, unknown>)
            : null,
      }))
      .sort((left, right) => right.snapshotId - left.snapshotId),
  };
}

export async function GET() {
  const client = createInternalDbClient();

  try {
    await client.connect();

    let payload = await client.call('JSON.GET', SNAPSHOT_KEY, '.');
    if (payload == null) {
      payload = await client.call('JSON.GET', LEGACY_SNAPSHOT_KEY, '.');
    }

    return NextResponse.json(parsePayload(payload), { status: 200 });
  } catch {
    return NextResponse.json(EMPTY_RESPONSE, { status: 200 });
  } finally {
    client.disconnect();
  }
}
