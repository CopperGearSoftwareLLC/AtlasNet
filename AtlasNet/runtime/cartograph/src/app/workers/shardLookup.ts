import type {
  ShardPlacementTelemetry,
  WorkerContainerTelemetry,
} from '../shared/cartographTypes';

const UUID_PATTERN =
  /^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i;

function normalizeShardId(value: unknown): string {
  const text = String(value ?? '').replace(/\0/g, '').trim();
  if (!text) {
    return '';
  }
  if (text.startsWith('eShard ')) {
    return text;
  }
  if (UUID_PATTERN.test(text)) {
    return `eShard ${text}`;
  }
  return text;
}

function extractPodName(container: WorkerContainerTelemetry): string | null {
  const fullName = String(container.name || '').trim();
  if (!fullName) {
    return null;
  }
  const slashIndex = fullName.indexOf('/');
  if (slashIndex <= 0) {
    return null;
  }
  const podName = fullName.slice(0, slashIndex).trim();
  return podName || null;
}

function collectContainerShardLookupCandidates(
  container: WorkerContainerTelemetry
): string[] {
  const out = new Set<string>();
  const fullName = String(container.name || '').trim();
  if (fullName) {
    out.add(fullName);
  }

  const podName = extractPodName(container);
  if (podName) {
    out.add(podName);
  }

  return Array.from(out.values());
}

export function buildShardIdLookup(
  placement: ShardPlacementTelemetry[]
): Map<string, string> {
  const out = new Map<string, string>();
  for (const row of placement) {
    const podName = String(row.podName || '').trim();
    const shardId = normalizeShardId(row.shardId);
    if (!podName || !shardId || out.has(podName)) {
      continue;
    }
    out.set(podName, shardId);
  }
  return out;
}

export function resolveContainerShardId(
  container: WorkerContainerTelemetry,
  shardIdLookup: Map<string, string>
): string | null {
  const candidates = collectContainerShardLookupCandidates(container);
  for (const candidate of candidates) {
    const shardId = shardIdLookup.get(candidate);
    if (shardId) {
      return shardId;
    }
  }
  return null;
}
