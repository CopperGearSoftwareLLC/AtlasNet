import Redis from 'ioredis';
import { NextResponse } from 'next/server';

const CURRENT_KEY = 'Heuristic:Type';
const DESIRED_KEY = 'Heuristic:DesiredType';
const CONNECT_TIMEOUT_MS = 700;
const ALLOWED_TYPES = new Set([
  'GridCell',
  'Quadtree',
  'Voronoi',
  'HotspotVoronoi',
  'LlmVoronoi',
]);

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

async function readState(client: Redis) {
  const [currentRaw, desiredRaw] = await Promise.all([
    client.get(CURRENT_KEY),
    client.get(DESIRED_KEY),
  ]);
  const currentHeuristicType = normalizeType(currentRaw);
  const desiredHeuristicType =
    normalizeType(desiredRaw) ?? currentHeuristicType ?? null;

  return {
    currentHeuristicType,
    desiredHeuristicType,
    allowedHeuristicTypes: Array.from(ALLOWED_TYPES),
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
    const body = (await request.json()) as { heuristicType?: unknown };
    const heuristicType = normalizeType(body?.heuristicType);
    if (!heuristicType || !ALLOWED_TYPES.has(heuristicType)) {
      return NextResponse.json(
        { error: 'Invalid heuristicType.' },
        { status: 400 }
      );
    }

    await client.connect();
    await client.set(DESIRED_KEY, heuristicType);
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
