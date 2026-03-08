import { respondWithNativeArray } from '../../shared/nativeRoute';

const TIMEOUT_MS = 12000;

export async function GET() {
  return respondWithNativeArray('/shard-placement', TIMEOUT_MS);
}
