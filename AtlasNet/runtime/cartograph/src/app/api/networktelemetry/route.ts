import { respondWithNativeArray } from '../../shared/nativeRoute';

const TIMEOUT_MS = 500;

export async function GET() {
  return respondWithNativeArray('/networktelemetry', TIMEOUT_MS);
}
