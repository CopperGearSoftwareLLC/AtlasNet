import { NextResponse } from 'next/server';
import { fetchNativeJson } from '../../lib/server/nativeClient';

const TIMEOUT_MS = 6000;

export async function GET(req: Request) {
  const reqUrl = new URL(req.url);
  const collectionMode = (
    reqUrl.searchParams.get('collectionMode') ??
    reqUrl.searchParams.get('mode') ??
    ''
  ).trim();

  const data = await fetchNativeJson<unknown[]>({
    path: '/authoritytelemetry',
    timeoutMs: TIMEOUT_MS,
    query:
      collectionMode.length > 0
        ? {
            collectionMode,
          }
        : undefined,
  });
  return NextResponse.json(Array.isArray(data) ? data : [], { status: 200 });
}
