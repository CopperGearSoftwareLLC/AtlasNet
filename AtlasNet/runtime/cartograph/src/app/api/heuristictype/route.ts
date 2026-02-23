import { NextResponse } from 'next/server';
import { fetchNativeJson } from '../../lib/server/nativeClient';

const TIMEOUT_MS = 1500;

interface HeuristicTypePayload {
  heuristicType?: unknown;
}

export async function GET() {
  const data = await fetchNativeJson<HeuristicTypePayload>({
    path: '/heuristictype',
    timeoutMs: TIMEOUT_MS,
  });

  const heuristicType =
    data && typeof data === 'object' && typeof data.heuristicType === 'string'
      ? data.heuristicType.trim()
      : '';

  return NextResponse.json(
    {
      heuristicType: heuristicType.length > 0 ? heuristicType : null,
    },
    { status: 200 }
  );
}
