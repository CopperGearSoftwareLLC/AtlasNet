import { NextResponse } from 'next/server';
import { normalizeWorkersSnapshot } from '../../workers/workersSnapshot';
import { fetchNativeJson } from '../../shared/nativeClient';

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
