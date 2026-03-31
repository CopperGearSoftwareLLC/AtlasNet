import { NextResponse } from 'next/server';
import { normalizeHeuristicShapes } from '../../map/core/heuristicShapeMapper';
import { fetchNativeJson } from '../../shared/nativeClient';

const TIMEOUT_MS = 700;

export async function GET() {
  const rawShapes = await fetchNativeJson<unknown>({
    path: '/heuristic',
    timeoutMs: TIMEOUT_MS,
  });
  if (rawShapes == null) {
    return NextResponse.json(
      { error: 'Failed to fetch shapes' },
      { status: 500 }
    );
  }
  return NextResponse.json(normalizeHeuristicShapes(rawShapes), { status: 200 });
}
