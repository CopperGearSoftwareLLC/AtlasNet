// /app/api/heuristicfetch/route.ts
import { NextResponse } from 'next/server';
import { normalizeHeuristicShapes } from '../../lib/server/heuristicShapeMapper';
import { fetchNativeJson } from '../../lib/server/nativeClient';

const TIMEOUT_MS = 700;

export async function GET(req: Request) {
  const reqUrl = new URL(req.url);
  const collectionMode = (
    reqUrl.searchParams.get('collectionMode') ??
    reqUrl.searchParams.get('mode') ??
    ''
  ).trim();

  const rawShapes = await fetchNativeJson<unknown>({
    path: '/heuristic',
    timeoutMs: TIMEOUT_MS,
    query:
      collectionMode.length > 0
        ? {
            collectionMode,
          }
        : undefined,
  });
  if (rawShapes == null) {
    return NextResponse.json(
      { error: 'Failed to fetch shapes' },
      { status: 500 }
    );
  }
  return NextResponse.json(normalizeHeuristicShapes(rawShapes), { status: 200 });
}
