import { NextResponse } from 'next/server';
import { fetchNativeJson } from '../../lib/server/nativeClient';

const TIMEOUT_MS = 12000;

export async function GET() {
  const payload = await fetchNativeJson<unknown>({
    path: '/shard-placement',
    timeoutMs: TIMEOUT_MS,
  });
  if (!Array.isArray(payload)) {
    return NextResponse.json([], { status: 200 });
  }
  return NextResponse.json(payload, { status: 200 });
}
