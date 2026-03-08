import { NextResponse } from 'next/server';
import { fetchNativeJson } from '../../shared/nativeClient';

const TIMEOUT_MS = 6000;

export async function GET() {
  const data = await fetchNativeJson<unknown[]>({
    path: '/transfermanifest',
    timeoutMs: TIMEOUT_MS,
  });

  return NextResponse.json(Array.isArray(data) ? data : [], { status: 200 });
}
