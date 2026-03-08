import { NextResponse } from 'next/server';
import { fetchNativeJson } from './nativeClient';

export async function respondWithNativeArray(
  path: string,
  timeoutMs: number
) {
  const data = await fetchNativeJson<unknown>({
    path,
    timeoutMs,
  });
  return NextResponse.json(Array.isArray(data) ? data : [], { status: 200 });
}
