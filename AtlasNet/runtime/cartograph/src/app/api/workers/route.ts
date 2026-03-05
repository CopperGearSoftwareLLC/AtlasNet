import { NextResponse } from 'next/server';
import { normalizeWorkersSnapshot } from '../../lib/server/workersSnapshot';
import { fetchNativeJson } from '../../lib/server/nativeClient';

const TIMEOUT_MS = 12000;

export async function GET() {
  const payload = await fetchNativeJson<unknown>({
    path: '/workers',
    timeoutMs: TIMEOUT_MS,
  });
  if (payload == null) {
    return NextResponse.json({ error: 'Workers snapshot unavailable' }, { status: 502 });
  }
  return NextResponse.json(normalizeWorkersSnapshot(payload), { status: 200 });
}
