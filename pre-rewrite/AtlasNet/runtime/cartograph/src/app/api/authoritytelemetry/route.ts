import { respondWithNativeArray } from '../../shared/nativeRoute';

const TIMEOUT_MS = 700;

export async function GET() {
  return respondWithNativeArray('/authoritytelemetry', TIMEOUT_MS);
}
