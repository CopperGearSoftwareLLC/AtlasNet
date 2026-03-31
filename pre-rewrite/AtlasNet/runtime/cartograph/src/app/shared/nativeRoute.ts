import { NextResponse } from 'next/server';
import { fetchNativeJson } from './nativeClient';

export async function respondWithNativeArray(
  path: string,
  timeoutMs: number,
  query?: Record<string, string | number | boolean | null | undefined>
) {
  const data = await fetchNativeJson<unknown>({
    path,
    timeoutMs,
    query,
  });
  return NextResponse.json(Array.isArray(data) ? data : [], { status: 200 });
}
