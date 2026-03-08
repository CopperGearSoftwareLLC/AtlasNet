import { NextResponse } from 'next/server';
import type { ShardTelemetry } from '../../shared/cartographTypes';
import { fetchNativeJson } from '../../shared/nativeClient';

const TIMEOUT_MS = 500;

export async function GET() {
  const data = await fetchNativeJson<ShardTelemetry[]>({
    path: '/networktelemetry',
    timeoutMs: TIMEOUT_MS,
  });
  return NextResponse.json(Array.isArray(data) ? data : [], { status: 200 });
}
