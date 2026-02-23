import { NextResponse } from 'next/server';
import { normalizeWorkersSnapshot } from '../../lib/server/workersSnapshot';
import { fetchNativeJson } from '../../lib/server/nativeClient';

const TIMEOUT_MS = 7000;

export async function GET() {
  const payload = await fetchNativeJson<unknown>({
    path: '/workers',
    timeoutMs: TIMEOUT_MS,
  });
  return NextResponse.json(normalizeWorkersSnapshot(payload), { status: 200 });
}
