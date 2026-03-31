import Redis from 'ioredis';
import { NextResponse } from 'next/server';

const CURRENT_KEY = 'Heuristic:Type';
const DESIRED_KEY = 'Heuristic:DesiredType';
const RECOMPUTE_MODE_KEY = 'Heuristic:RecomputeMode';
const RECOMPUTE_INTERVAL_MS_KEY = 'Heuristic:RecomputeIntervalMs';
const RECOMPUTE_REQUEST_KEY = 'Heuristic:RecomputeRequest';
const CONNECT_TIMEOUT_MS = 700;
const DEFAULT_RECOMPUTE_INTERVAL_MS = 5000;
const ALLOWED_TYPES = new Set([
  'GridCell',
  'Quadtree',
  'Voronoi',
  'HotspotVoronoi',
  'LlmVoronoi',
]);
const ALLOWED_RECOMPUTE_MODES = new Set(['interval', 'manual', 'load'] as const);

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

function normalizeType(value: unknown): string | null {
  if (typeof value !== 'string') {
    return null;
  }
  const text = value.trim();
  return text.length > 0 ? text : null;
}

function normalizeRecomputeMode(
  value: unknown
): 'interval' | 'manual' | 'load' | null {
  if (typeof value !== 'string') {
    return null;
  }
  const text = value.trim().toLowerCase();
  if (!ALLOWED_RECOMPUTE_MODES.has(text as 'interval' | 'manual' | 'load')) {
    return null;
  }
  return text as 'interval' | 'manual' | 'load';
}

function normalizeRecomputeIntervalMs(value: unknown): number | null {
  const parsed = Number(value);
  if (!Number.isFinite(parsed)) {
    return null;
  }
  const rounded = Math.round(parsed);
  if (rounded < 1) {
    return null;
  }
  return rounded;
}

async function readState(client: Redis) {
  const [currentRaw, desiredRaw, recomputeModeRaw, recomputeIntervalRaw] = await Promise.all([
    client.get(CURRENT_KEY),
    client.get(DESIRED_KEY),
    client.get(RECOMPUTE_MODE_KEY),
    client.get(RECOMPUTE_INTERVAL_MS_KEY),
  ]);
  const currentHeuristicType = normalizeType(currentRaw);
  const desiredHeuristicType =
    normalizeType(desiredRaw) ?? currentHeuristicType ?? null;
  const recomputeMode =
    normalizeRecomputeMode(recomputeModeRaw) ?? 'interval';
  const recomputeIntervalMs =
    normalizeRecomputeIntervalMs(recomputeIntervalRaw) ?? DEFAULT_RECOMPUTE_INTERVAL_MS;

  return {
    currentHeuristicType,
    desiredHeuristicType,
    allowedHeuristicTypes: Array.from(ALLOWED_TYPES),
    recomputeMode,
    recomputeIntervalMs,
    loadState: 'stubbed' as const,
  };
}

export async function GET() {
  const client = createInternalDbClient();

  try {
    await client.connect();
    return NextResponse.json(await readState(client), { status: 200 });
  } catch {
    return NextResponse.json(
      {
        currentHeuristicType: null,
        desiredHeuristicType: null,
        allowedHeuristicTypes: Array.from(ALLOWED_TYPES),
        recomputeMode: 'interval',
        recomputeIntervalMs: DEFAULT_RECOMPUTE_INTERVAL_MS,
        loadState: 'stubbed',
      },
      { status: 200 }
    );
  } finally {
    client.disconnect();
  }
}

export async function POST(request: Request) {
  const client = createInternalDbClient();

  try {
    const body = (await request.json()) as {
      heuristicType?: unknown;
      recomputeMode?: unknown;
      recomputeIntervalMs?: unknown;
      requestRecompute?: unknown;
    };
    const heuristicType = normalizeType(body?.heuristicType);
    const recomputeMode = normalizeRecomputeMode(body?.recomputeMode);
    const recomputeIntervalMs = normalizeRecomputeIntervalMs(body?.recomputeIntervalMs);
    const requestRecompute = body?.requestRecompute === true;

    if (
      body?.heuristicType !== undefined &&
      (!heuristicType || !ALLOWED_TYPES.has(heuristicType))
    ) {
      return NextResponse.json(
        { error: 'Invalid heuristicType.' },
        { status: 400 }
      );
    }
    if (body?.recomputeMode !== undefined && !recomputeMode) {
      return NextResponse.json(
        { error: 'Invalid recomputeMode.' },
        { status: 400 }
      );
    }
    if (body?.recomputeIntervalMs !== undefined && recomputeIntervalMs == null) {
      return NextResponse.json(
        { error: 'Invalid recomputeIntervalMs.' },
        { status: 400 }
      );
    }
    if (
      body?.heuristicType === undefined &&
      body?.recomputeMode === undefined &&
      body?.recomputeIntervalMs === undefined &&
      !requestRecompute
    ) {
      return NextResponse.json(
        { error: 'No heuristic control changes were provided.' },
        { status: 400 }
      );
    }

    await client.connect();
    if (heuristicType) {
      await client.set(DESIRED_KEY, heuristicType);
    }
    if (recomputeMode) {
      await client.set(RECOMPUTE_MODE_KEY, recomputeMode);
    }
    if (recomputeIntervalMs != null) {
      await client.set(RECOMPUTE_INTERVAL_MS_KEY, String(recomputeIntervalMs));
    }
    if (requestRecompute) {
      await client.set(RECOMPUTE_REQUEST_KEY, String(Date.now()));
    }
    return NextResponse.json(await readState(client), { status: 200 });
  } catch {
    return NextResponse.json(
      { error: 'Failed to update heuristic control.' },
      { status: 500 }
    );
  } finally {
    client.disconnect();
  }
}
