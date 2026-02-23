const Redis = require('ioredis');
const { formatUuid } = require('./RevertByteOptimation/format');
const { buildNetworkTelemetry } = require('./networkTelemetry');
const { getDatabaseTargets, SNAPSHOT_CONNECT_TIMEOUT_MS } = require('../config');

const AUTHORITY_TELEMETRY_KEY = 'Authority_Telemetry';
const HEALTH_PING_KEY = 'Health_Ping';
const NETWORK_TELEMETRY_KEY = 'Network_Telemetry';
const HEURISTIC_MANIFEST_KEY = 'HeuristicManifest';

const AUTHORITY_TELEMETRY_COLUMN_COUNT = 7;
const NETWORK_TELEMETRY_COLUMN_COUNT = 13;
const GRID_SHAPE_SERIALIZED_SIZE_BYTES = 28;
const CLAIMED_OWNER_MAP_CACHE_TTL_MS = 500;

const NETWORK_IDENTITY_TYPE_BY_CODE = {
  0: 'eInvalid',
  1: 'eShard',
  2: 'eWatchDog',
  3: 'eCartograph',
  4: 'eGameClient',
  6: 'eProxy',
  7: 'eAtlasNetInitial',
};

let internalDbClient = null;
let internalDbConnectPromise = null;
let heuristicClaimedOwnerCache = null;
let heuristicClaimedOwnerCacheAtMs = 0;

function getInternalDatabaseTarget() {
  const targets = getDatabaseTargets();
  const internalTarget = targets.find((target) => target.id === 'internal');
  return internalTarget || targets[0] || null;
}

function createRedisClient(target, connectTimeout) {
  return new Redis({
    host: target.host,
    port: target.port,
    lazyConnect: true,
    connectTimeout,
    maxRetriesPerRequest: 0,
    enableOfflineQueue: false,
    retryStrategy: null,
  });
}

async function getInternalDatabaseClient() {
  const target = getInternalDatabaseTarget();
  if (!target) {
    return null;
  }

  if (!internalDbClient) {
    internalDbClient = createRedisClient(target, SNAPSHOT_CONNECT_TIMEOUT_MS);
  }

  const status = String(internalDbClient.status || '');
  if (status === 'ready') {
    return internalDbClient;
  }

  if (!internalDbConnectPromise) {
    internalDbConnectPromise = internalDbClient
      .connect()
      .catch((err) => {
        internalDbClient = null;
        throw err;
      })
      .finally(() => {
        internalDbConnectPromise = null;
      });
  }

  await internalDbConnectPromise;
  return internalDbClient;
}

async function withInternalDatabase(readFn) {
  const client = await getInternalDatabaseClient();
  if (!client) {
    return null;
  }
  try {
    return await readFn(client);
  } catch (err) {
    try {
      client.disconnect();
    } catch {}
    internalDbClient = null;
    throw err;
  }
}

function parseTabSeparatedColumns(payload, expectedColumnCount) {
  if (typeof payload !== 'string' || payload.length === 0) {
    return null;
  }
  const columns = payload.split('\t');
  if (columns.length !== expectedColumnCount) {
    return null;
  }
  return columns;
}

function decodeNetworkIdentity(raw) {
  const buffer = Buffer.isBuffer(raw)
    ? raw
    : Buffer.from(typeof raw === 'string' ? raw : String(raw));

  if (buffer.length < 20) {
    return String(raw ?? '');
  }

  const typeCode = buffer.readUInt32BE(0);
  const typeName =
    NETWORK_IDENTITY_TYPE_BY_CODE[typeCode] || `eUnknown(${typeCode})`;
  const id = formatUuid(buffer.subarray(4, 20));
  const isNilId = /^0{8}-0{4}-0{4}-0{4}-0{12}$/i.test(id);
  return isNilId ? typeName : `${typeName} ${id}`;
}

function normalizeRedisJsonPayload(payload) {
  if (payload == null) {
    return null;
  }

  const raw = String(payload);
  if (raw.length === 0 || raw === 'null') {
    return null;
  }

  try {
    let parsed = JSON.parse(raw);
    if (Array.isArray(parsed)) {
      parsed = parsed[0];
    }
    return parsed && typeof parsed === 'object' ? parsed : null;
  } catch {
    return null;
  }
}

function decodeGridShapeBounds(base64Value) {
  if (typeof base64Value !== 'string' || base64Value.length === 0) {
    return null;
  }

  let raw = null;
  try {
    raw = Buffer.from(base64Value, 'base64');
  } catch {
    return null;
  }

  if (!raw || raw.length < GRID_SHAPE_SERIALIZED_SIZE_BYTES) {
    return null;
  }

  try {
    return {
      id: raw.readUInt32BE(0),
      min: {
        x: raw.readFloatBE(4),
        y: raw.readFloatBE(8),
        z: raw.readFloatBE(12),
      },
      max: {
        x: raw.readFloatBE(16),
        y: raw.readFloatBE(20),
        z: raw.readFloatBE(24),
      },
    };
  } catch {
    return null;
  }
}

function toRectangleShape(bounds, ownerId, color) {
  const minX = Number(bounds.min?.x);
  const minY = Number(bounds.min?.y);
  const maxX = Number(bounds.max?.x);
  const maxY = Number(bounds.max?.y);
  if (
    !Number.isFinite(minX) ||
    !Number.isFinite(minY) ||
    !Number.isFinite(maxX) ||
    !Number.isFinite(maxY)
  ) {
    return null;
  }

  return {
    id: String(bounds.id),
    ownerId: ownerId || '',
    type: 1,
    position: {
      x: (minX + maxX) / 2,
      y: (minY + maxY) / 2,
    },
    radius: 0,
    size: {
      x: Math.abs(maxX - minX),
      y: Math.abs(maxY - minY),
    },
    color,
    vertices: [],
  };
}

function getManifestEntryValues(section) {
  if (Array.isArray(section)) {
    return section;
  }
  if (section && typeof section === 'object') {
    return Object.values(section);
  }
  return [];
}

async function readAuthorityTelemetryFromDatabase() {
  return (
    (await withInternalDatabase(async (client) => {
      const all = await client.hgetall(AUTHORITY_TELEMETRY_KEY);
      const rows = [];

      for (const [entityField, payload] of Object.entries(all || {})) {
        const columns = parseTabSeparatedColumns(
          String(payload),
          AUTHORITY_TELEMETRY_COLUMN_COUNT
        );
        if (!columns) {
          continue;
        }
        rows.push([String(entityField), ...columns]);
      }

      rows.sort((left, right) => left[0].localeCompare(right[0]));
      return rows;
    })) || []
  );
}

async function readNetworkTelemetryFromDatabase() {
  return (
    (await withInternalDatabase(async (client) => {
      const idsRaw =
        typeof client.hkeysBuffer === 'function'
          ? await client.hkeysBuffer(HEALTH_PING_KEY)
          : await client.hkeys(HEALTH_PING_KEY);

      const liveShardIds = [];
      if (Array.isArray(idsRaw)) {
        for (const rawId of idsRaw) {
          const decoded = decodeNetworkIdentity(rawId).trim();
          if (decoded.length > 0) {
            liveShardIds.push(decoded);
          }
        }
      }

      const allTelemetry = await client.hgetall(NETWORK_TELEMETRY_KEY);
      const rows = [];
      for (const [shardId, payload] of Object.entries(allTelemetry || {})) {
        const lines = String(payload).split(/\r?\n/);
        for (const line of lines) {
          if (!line) {
            continue;
          }
          const columns = parseTabSeparatedColumns(
            line,
            NETWORK_TELEMETRY_COLUMN_COUNT
          );
          if (!columns) {
            continue;
          }
          rows.push([String(shardId), ...columns]);
        }
      }

      return buildNetworkTelemetry(liveShardIds, rows);
    })) || []
  );
}

async function readHeuristicShapesFromDatabase() {
  return (
    (await withInternalDatabase(async (client) => {
      const payload = await client.call('JSON.GET', HEURISTIC_MANIFEST_KEY, '.');
      const manifest = normalizeRedisJsonPayload(payload);
      if (!manifest) {
        return [];
      }

      const shapes = [];

      for (const value of getManifestEntryValues(manifest.Pending)) {
        if (!value || typeof value !== 'object') {
          continue;
        }
        const bounds = decodeGridShapeBounds(value.BoundsData64);
        if (!bounds) {
          continue;
        }
        const shape = toRectangleShape(bounds, '', 'rgba(255, 149, 100, 1)');
        if (shape) {
          shapes.push(shape);
        }
      }

      for (const value of getManifestEntryValues(manifest.Claimed)) {
        if (!value || typeof value !== 'object') {
          continue;
        }
        const bounds = decodeGridShapeBounds(value.BoundsData64);
        if (!bounds) {
          continue;
        }

        let ownerId =
          typeof value.OwnerName === 'string' ? value.OwnerName : '';
        if (!ownerId && typeof value.Owner64 === 'string') {
          try {
            ownerId = decodeNetworkIdentity(Buffer.from(value.Owner64, 'base64'));
          } catch {
            ownerId = '';
          }
        }

        const shape = toRectangleShape(
          bounds,
          ownerId,
          'rgba(100, 255, 149, 1)'
        );
        if (shape) {
          shapes.push(shape);
        }
      }

      return shapes;
    })) || []
  );
}

async function readHeuristicClaimedOwnersFromDatabase() {
  const now = Date.now();
  if (
    heuristicClaimedOwnerCache &&
    now - heuristicClaimedOwnerCacheAtMs < CLAIMED_OWNER_MAP_CACHE_TTL_MS
  ) {
    return heuristicClaimedOwnerCache;
  }

  const owners =
    (await withInternalDatabase(async (client) => {
      const payload = await client.call('JSON.GET', HEURISTIC_MANIFEST_KEY, '.');
      const manifest = normalizeRedisJsonPayload(payload);
      if (!manifest) {
        return null;
      }

      const ownerByBoundId = {};
      for (const value of getManifestEntryValues(manifest.Claimed)) {
        if (!value || typeof value !== 'object') {
          continue;
        }

        const boundId = String(value.ID ?? value.id ?? '').trim();
        if (!boundId) {
          continue;
        }

        let ownerId =
          typeof value.OwnerName === 'string' ? value.OwnerName.trim() : '';
        if (!ownerId && typeof value.Owner64 === 'string') {
          try {
            ownerId = decodeNetworkIdentity(Buffer.from(value.Owner64, 'base64')).trim();
          } catch {
            ownerId = '';
          }
        }

        if (ownerId) {
          ownerByBoundId[boundId] = ownerId;
        }
      }

      heuristicClaimedOwnerCache = ownerByBoundId;
      heuristicClaimedOwnerCacheAtMs = Date.now();
      return ownerByBoundId;
    })) || null;

  if (owners && typeof owners === 'object') {
    return owners;
  }

  return heuristicClaimedOwnerCache || {};
}

async function readHeuristicTypeFromDatabase() {
  return (
    (await withInternalDatabase(async (client) => {
      const payload = await client.call(
        'JSON.GET',
        HEURISTIC_MANIFEST_KEY,
        '.HeuristicType'
      );

      if (payload == null) {
        return null;
      }

      const raw = String(payload).trim();
      if (!raw || raw === 'null') {
        return null;
      }

      try {
        let parsed = JSON.parse(raw);
        if (Array.isArray(parsed)) {
          parsed = parsed[0];
        }
        if (typeof parsed === 'string') {
          const text = parsed.trim();
          return text.length > 0 ? text : null;
        }
      } catch {}

      const unquoted = raw.replace(/^"(.*)"$/, '$1').trim();
      return unquoted.length > 0 ? unquoted : null;
    })) || null
  );
}

module.exports = {
  readAuthorityTelemetryFromDatabase,
  readNetworkTelemetryFromDatabase,
  readHeuristicShapesFromDatabase,
  readHeuristicClaimedOwnersFromDatabase,
  readHeuristicTypeFromDatabase,
};
