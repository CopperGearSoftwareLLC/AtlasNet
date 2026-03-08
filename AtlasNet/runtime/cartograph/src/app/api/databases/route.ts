import { NextResponse } from 'next/server';
import { normalizeDatabaseSnapshot } from '../../shared/databaseSnapshot';
import { fetchNativeJson } from '../../shared/nativeClient';
import { parseOptionalBooleanFlag } from '../../shared/queryParams';

const TIMEOUT_MS = 5000;

export async function GET(req: Request) {
  const reqUrl = new URL(req.url);
  const source = (reqUrl.searchParams.get('source') ?? '').trim();
  const decodeSerialized = parseOptionalBooleanFlag(
    reqUrl.searchParams.get('decodeSerialized') ?? ''
  );

  const query =
    source || decodeSerialized != null
      ? {
          source: source || undefined,
          decodeSerialized,
        }
      : undefined;

  const payload = await fetchNativeJson<unknown>({
    path: '/databases',
    timeoutMs: TIMEOUT_MS,
    query,
  });
  if (payload == null) {
    return NextResponse.json(
      { error: 'Database snapshot unavailable' },
      { status: 502 }
    );
  }

  return NextResponse.json(normalizeDatabaseSnapshot(payload), { status: 200 });
}
