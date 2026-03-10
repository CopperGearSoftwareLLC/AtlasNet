import type { ConnectionTelemetry } from './cartographTypes';

export function computeAveragePingMs(
  connections: Array<Pick<ConnectionTelemetry, 'pingMs'>>
): number | null {
  if (!Array.isArray(connections) || connections.length === 0) {
    return null;
  }

  let sum = 0;
  let count = 0;

  for (const connection of connections) {
    const pingMs = connection?.pingMs;
    if (!Number.isFinite(pingMs) || pingMs == null || pingMs < 0) {
      continue;
    }
    sum += pingMs;
    count += 1;
  }

  if (count === 0) {
    return null;
  }

  return sum / count;
}

export function formatPingMs(value: number | null | undefined): string {
  if (!Number.isFinite(value) || value == null || value < 0) {
    return '-';
  }
  return `${value.toFixed(1)} ms`;
}
