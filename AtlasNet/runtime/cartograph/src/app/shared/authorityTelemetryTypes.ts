import type { AuthorityEntityTelemetry } from './cartographTypes';

export type { AuthorityEntityTelemetry } from './cartographTypes';

function toNumber(value: unknown, fallback = 0): number {
  const n = Number(value);
  return Number.isFinite(n) ? n : fallback;
}

// Accepts row format:
// [entityId, ownerId, world, x, y, z, isClient, clientId]
// or object-like payload with matching keys.
export function parseAuthorityRows(raw: unknown): AuthorityEntityTelemetry[] {
  if (!Array.isArray(raw)) return [];

  const rows: AuthorityEntityTelemetry[] = [];
  for (const item of raw) {
    if (Array.isArray(item) && item.length >= 8) {
      rows.push({
        entityId: String(item[0]),
        ownerId: String(item[1]),
        world: toNumber(item[2]),
        x: toNumber(item[3]),
        y: toNumber(item[4]),
        z: toNumber(item[5]),
        isClient: String(item[6]) === '1' || item[6] === true,
        clientId: String(item[7]),
      });
      continue;
    }

    if (item && typeof item === 'object') {
      const obj = item as Record<string, unknown>;
      rows.push({
        entityId: String(obj.entityId ?? ''),
        ownerId: String(obj.ownerId ?? obj.owner ?? ''),
        world: toNumber(obj.world),
        x: toNumber(obj.x),
        y: toNumber(obj.y),
        z: toNumber(obj.z),
        isClient: Boolean(obj.isClient),
        clientId: String(obj.clientId ?? ''),
      });
    }
  }
  return rows.filter((r) => r.entityId.length > 0 && r.ownerId.length > 0);
}
