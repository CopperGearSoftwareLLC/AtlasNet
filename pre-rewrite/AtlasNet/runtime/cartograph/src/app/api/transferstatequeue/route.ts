import { respondWithNativeArray } from '../../shared/nativeRoute';

const TIMEOUT_MS = 700;

export async function GET() {
  return respondWithNativeArray('/transferstatequeue', TIMEOUT_MS);
}
