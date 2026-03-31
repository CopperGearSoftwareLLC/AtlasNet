import { respondWithNativeArray } from '../../shared/nativeRoute';

const TIMEOUT_MS = 500;

export async function GET(request: Request) {
  const url = new URL(request.url);
  const liveIds = url.searchParams.get('liveIds');
  return respondWithNativeArray('/networktelemetry', TIMEOUT_MS, {
    liveIds: liveIds || undefined,
  });
}
