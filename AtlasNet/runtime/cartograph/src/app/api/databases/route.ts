'use server';

import { NextResponse } from 'next/server';
import type { DatabaseSnapshotResponse } from '../../lib/databaseTypes';

const NATIVE_URL = 'http://127.0.0.1:4000/databases';
const TIMEOUT_MS = 1000;

export async function GET(req: Request) {
  const ac = new AbortController();
  const timeoutId = setTimeout(() => ac.abort(), TIMEOUT_MS);

  try {
    const reqUrl = new URL(req.url);
    const source = (reqUrl.searchParams.get('source') ?? '').trim();

    const upstream = new URL(NATIVE_URL);
    if (source.length > 0) {
      upstream.searchParams.set('source', source);
    }

    const response = await fetch(upstream.toString(), {
      cache: 'no-store',
      signal: ac.signal,
    });

    if (!response.ok) {
      return NextResponse.json(
        { sources: [], selectedSource: null, records: [] },
        { status: 200 }
      );
    }

    const data = (await response.json()) as DatabaseSnapshotResponse;
    if (!data || typeof data !== 'object') {
      return NextResponse.json(
        { sources: [], selectedSource: null, records: [] },
        { status: 200 }
      );
    }

    return NextResponse.json(
      {
        sources: Array.isArray(data.sources) ? data.sources : [],
        selectedSource:
          typeof data.selectedSource === 'string' ? data.selectedSource : null,
        records: Array.isArray(data.records) ? data.records : [],
      },
      { status: 200 }
    );
  } catch {
    return NextResponse.json(
      { sources: [], selectedSource: null, records: [] },
      { status: 200 }
    );
  } finally {
    clearTimeout(timeoutId);
  }
}
