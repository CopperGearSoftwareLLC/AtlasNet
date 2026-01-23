import { NextResponse } from 'next/server';
import type { ShardTelemetry } from '../../lib/networkTelemetryTypes';

export async function GET() {
  const response = await fetch('http://localhost:4000/networktelemetry', {
    cache: 'no-store',
  });

  if (!response.ok) {
    return NextResponse.json(
      { error: 'native-server failed' },
      { status: 502 }
    );
  }

  const data = (await response.json()) as ShardTelemetry[];
  return NextResponse.json(data);
}