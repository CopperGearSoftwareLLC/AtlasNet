import { NextResponse } from 'next/server';
import { normalizeDatabaseSnapshot } from '../../lib/server/databaseSnapshot';
import { fetchNativeJson } from '../../lib/server/nativeClient';

const TIMEOUT_MS = 1000;

function parseOptionalBooleanFlag(raw: string): boolean | undefined {
  const normalized = raw.trim().toLowerCase();
  if (normalized.length === 0) {
    return undefined;
  }
  if (
    normalized === '1' ||
    normalized === 'true' ||
    normalized === 'on' ||
    normalized === 'yes'
  ) {
    return true;
  }
  if (
    normalized === '0' ||
    normalized === 'false' ||
    normalized === 'off' ||
    normalized === 'no'
  ) {
    return false;
  }
  return undefined;
}

export async function GET(req: Request) {
  const reqUrl = new URL(req.url);
  const source = (reqUrl.searchParams.get('source') ?? '').trim();
  const decodeSerialized = parseOptionalBooleanFlag(
    reqUrl.searchParams.get('decodeSerialized') ??
      reqUrl.searchParams.get('decodeEntitySnapshots') ??
      ''
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
  return NextResponse.json(normalizeDatabaseSnapshot(payload), { status: 200 });
}
