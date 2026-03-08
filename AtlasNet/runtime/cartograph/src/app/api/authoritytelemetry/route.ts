import { respondWithNativeArray } from '../../shared/nativeRoute';

const TIMEOUT_MS = 6000;

export async function GET() {
  return respondWithNativeArray('/authoritytelemetry', TIMEOUT_MS);
}
