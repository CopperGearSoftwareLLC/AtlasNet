'use server';

import { NextResponse } from 'next/server';
import type { ShardTelemetry } from '../../lib/networkTelemetryTypes';


const NATIVE_URL = 'http://127.0.0.1:4000/networktelemetry';
const TIMEOUT_MS = 500; // keep short for dashboards

export async function GET() {
  const ac = new AbortController();
  const t = setTimeout(() => ac.abort(), TIMEOUT_MS);

  try {
    const response = await fetch(NATIVE_URL, {
      cache: 'no-store',
      signal: ac.signal,
    });

    if (!response.ok) {
      return NextResponse.json([], { status: 200 });
    }

    const data = await response.json() as ShardTelemetry[];
    return NextResponse.json(data, { status: 200 });
  } catch {
    // native server unreachable or timed out
    return NextResponse.json([], { status: 200 });
  } finally {
    clearTimeout(t);
  }
}