const Redis = require('ioredis');
const { formatUuid } = require('./serializedDecoders/format');
const { buildNetworkTelemetry } = require('./networkTelemetry');
const { getDatabaseTargets, SNAPSHOT_CONNECT_TIMEOUT_MS } = require('../config');

const AUTHORITY_TELEMETRY_KEY = 'Authority_Telemetry';
const HEALTH_PING_KEY = 'Health_Ping';
const NETWORK_TELEMETRY_KEY = 'Network_Telemetry';
const HEURISTIC_MANIFEST_KEY = 'HeuristicManifest';
const HEURISTIC_TYPE_KEY = 'Heuristic:Type';
const HEURISTIC_VERSION_KEY = 'Heuristic:Version';
const HEURISTIC_DATA_KEY = 'Heuristic:Data';
const HEURISTIC_OWNERSHIP_VERSION_KEY = 'Heuristic::Ownership:Version';
const HEURISTIC_OWNERSHIP_NID_TO_BOUND_MAP_KEY =
  'Heuristic::Ownership:NetID -> BoundID Map';
const SNAPSHOT_BOUNDIDS_TO_ENTITY_LIST_KEY = 'Snapshot:BoundIDs -> EntityList';
const SNAPSHOT_BOUNDIDS_TO_TRANSFORMS_KEY = 'Snapshot:BoundIDs -> Transforms';
const NODE_MANIFEST_SHARD_NODE_KEY = 'Node Manifest Shard_Node';
const TRANSFER_MANIFEST_KEY = 'Transfer::TransferManifest';

const AUTHORITY_TELEMETRY_COLUMN_COUNT = 7;
const NETWORK_TELEMETRY_COLUMN_COUNT = 13;
const GRID_SHAPE_SERIALIZED_SIZE_BYTES = 28;
const CLAIMED_OWNER_MAP_CACHE_TTL_MS = 500;
const TRANSFER_STAGE_SOURCE_STATES = new Set(['eNone', 'ePrepare', 'eUnknown']);
const VALID_TRANSFER_STAGES = new Set([
  'eNone',
  'ePrepare',
  'eReady',
  'eCommit',
  'eComplete',
]);

const NETWORK_IDENTITY_TYPE_BY_CODE = {
  0: 'eInvalid',
  1: 'eShard',
  2: 'eWatchDog',
  3: 'eCartograph',
  4: 'eGameClient',
  6: 'eProxy',
  7: 'eAtlasNetInitial',
};
const UUID_PATTERN =
  /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

const DEFAULT_INTERNAL_DB_CLIENT_SCOPE = 'default';
const internalDbClients = new Map();
const internalDbConnectPromises = new Map();
let heuristicClaimedOwnerCache = null;
let heuristicClaimedOwnerCacheAtMs = 0;
let heuristicClaimedOwnerCacheVersion = null;

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

function resolveInternalDbClientScope(rawScope) {
  const scope = String(rawScope ?? '').trim();
  return scope.length > 0 ? scope : DEFAULT_INTERNAL_DB_CLIENT_SCOPE;
}

function resetInternalDatabaseClient(scope, client) {
  try {
    if (client && typeof client.disconnect === 'function') {
      client.disconnect();
    }
  } catch {}
  internalDbClients.delete(scope);
  internalDbConnectPromises.delete(scope);
}

async function getInternalDatabaseClient(options = {}) {
  const scope = resolveInternalDbClientScope(options.clientScope);
  const target = getInternalDatabaseTarget();
  if (!target) {
    return null;
  }

  if (!internalDbClients.has(scope)) {
    internalDbClients.set(
      scope,
      createRedisClient(target, SNAPSHOT_CONNECT_TIMEOUT_MS)
    );
  }
  const client = internalDbClients.get(scope);
  if (!client) {
    return null;
  }

  const status = String(client.status || '');
  if (status === 'ready') {
    return client;
  }

  if (!internalDbConnectPromises.has(scope)) {
    internalDbConnectPromises.set(
      scope,
      client
      .connect()
      .catch((err) => {
        resetInternalDatabaseClient(scope, client);
        throw err;
      })
      .finally(() => {
        internalDbConnectPromises.delete(scope);
      })
    );
  }

  await internalDbConnectPromises.get(scope);
  return internalDbClients.get(scope) || null;
}

async function withInternalDatabase(readFn, options = {}) {
  const scope = resolveInternalDbClientScope(options.clientScope);
  const client = await getInternalDatabaseClient({ clientScope: scope });
  if (!client) {
    return null;
  }
  try {
    return await readFn(client);
  } catch (err) {
    resetInternalDatabaseClient(scope, client);
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

function decodeUtf8Text(raw) {
  if (raw == null) {
    return '';
  }
  if (Buffer.isBuffer(raw)) {
    return raw.toString('utf8');
  }
  return String(raw);
}

function normalizeText(raw) {
  return decodeUtf8Text(raw).replace(/\0/g, '').trim();
}

function normalizeHeuristicTypeName(raw) {
  const text = normalizeText(raw).toLowerCase();
  if (!text) {
    return null;
  }
  if (text === 'voronoi') {
    return 'Voronoi';
  }
  if (text === 'hotspotvoronoi' || text === 'ehotspotvoronoi') {
    return 'HotspotVoronoi';
  }
  if (text === 'llmvoronoi' || text === 'ellmvoronoi') {
    return 'LlmVoronoi';
  }
  if (text === 'gridcell' || text === 'grid' || text === 'egridcell') {
    return 'GridCell';
  }
  if (text === 'quadtree' || text === 'equadtree') {
    return 'Quadtree';
  }
  return normalizeText(raw) || null;
}

function isVoronoiHeuristicType(type) {
  return (
    type === 'Voronoi' ||
    type === 'HotspotVoronoi' ||
    type === 'LlmVoronoi'
  );
}

function parseBoundIdText(raw) {
  const text = normalizeText(raw);
  if (!text) {
    return null;
  }
  const match = text.match(/^\d+/);
  if (!match) {
    return null;
  }
  const parsed = Number(match[0]);
  if (!Number.isFinite(parsed) || parsed < 0) {
    return null;
  }
  return String(Math.floor(parsed));
}

function parseOwnerIdFromOwnershipField(rawField) {
  if (Buffer.isBuffer(rawField)) {
    if (rawField.length === 20) {
      const decoded = decodeNetworkIdentity(rawField).trim();
      if (decoded.length > 0) {
        return decoded;
      }
    }

    if (rawField.length === 16) {
      try {
        return `eShard ${formatUuid(rawField)}`;
      } catch {}
    }
  }

  const text = normalizeText(rawField);
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

function parseGridShapeBoundsRaw(raw, offset) {
  if (!Buffer.isBuffer(raw) || offset + GRID_SHAPE_SERIALIZED_SIZE_BYTES > raw.length) {
    return null;
  }
  try {
    return {
      nextOffset: offset + GRID_SHAPE_SERIALIZED_SIZE_BYTES,
      bound: {
        id: raw.readUInt32BE(offset),
        min: {
          x: raw.readFloatBE(offset + 4),
          y: raw.readFloatBE(offset + 8),
          z: raw.readFloatBE(offset + 12),
        },
        max: {
          x: raw.readFloatBE(offset + 16),
          y: raw.readFloatBE(offset + 20),
          z: raw.readFloatBE(offset + 24),
        },
      },
    };
  } catch {
    return null;
  }
}

function parseVoronoiShapeBoundsRaw(raw, offset) {
  if (!Buffer.isBuffer(raw) || offset + 8 > raw.length) {
    return null;
  }
  try {
    const id = raw.readUInt32BE(offset);
    const versionOrPointCount = raw.readUInt32BE(offset + 4);

    if (versionOrPointCount <= 2) {
      const version = versionOrPointCount;
      if (version !== 1 || offset + 20 > raw.length) {
        return null;
      }

      const site = {
        x: raw.readFloatBE(offset + 8),
        y: raw.readFloatBE(offset + 12),
      };
      const halfPlaneCount = raw.readUInt32BE(offset + 16);
      if (!Number.isFinite(halfPlaneCount) || halfPlaneCount < 0 || halfPlaneCount > 8192) {
        return null;
      }

      let cursor = offset + 20;
      const halfPlanes = [];
      for (let i = 0; i < halfPlaneCount; i += 1) {
        if (cursor + 12 > raw.length) {
          return null;
        }
        const nx = raw.readFloatBE(cursor);
        const ny = raw.readFloatBE(cursor + 4);
        const c = raw.readFloatBE(cursor + 8);
        if (!Number.isFinite(nx) || !Number.isFinite(ny) || !Number.isFinite(c)) {
          return null;
        }
        halfPlanes.push({ nx, ny, c });
        cursor += 12;
      }

      if (cursor + 4 > raw.length) {
        return null;
      }
      const pointCount = raw.readUInt32BE(cursor);
      cursor += 4;
      if (!Number.isFinite(pointCount) || pointCount < 0 || pointCount > 8192) {
        return null;
      }

      const points = [];
      for (let i = 0; i < pointCount; i += 1) {
        if (cursor + 8 > raw.length) {
          return null;
        }
        const x = raw.readFloatBE(cursor);
        const y = raw.readFloatBE(cursor + 4);
        if (!Number.isFinite(x) || !Number.isFinite(y)) {
          return null;
        }
        points.push({ x, y });
        cursor += 8;
      }

      return {
        nextOffset: cursor,
        bound: { id, site, halfPlanes, points },
      };
    }

    let cursor = offset + 8;
    const pointCount = versionOrPointCount;
    if (!Number.isFinite(pointCount) || pointCount < 3 || pointCount > 8192) {
      return null;
    }
    const points = [];
    for (let i = 0; i < pointCount; i += 1) {
      if (cursor + 8 > raw.length) {
        return null;
      }
      const x = raw.readFloatBE(cursor);
      const y = raw.readFloatBE(cursor + 4);
      if (!Number.isFinite(x) || !Number.isFinite(y)) {
        return null;
      }
      points.push({ x, y });
      cursor += 8;
    }

    return {
      nextOffset: cursor,
      bound: { id, points },
    };
  } catch {
    return null;
  }
}

function decodeHeuristicBoundsFromBinary(raw, heuristicType) {
  if (!Buffer.isBuffer(raw) || raw.length < 8) {
    return [];
  }

  let count = 0;
  try {
    count = Number(raw.readBigUInt64BE(0));
  } catch {
    return [];
  }

  if (!Number.isFinite(count) || count < 0 || count > 100000) {
    return [];
  }

  const type = normalizeHeuristicTypeName(heuristicType);
  const isVoronoi = isVoronoiHeuristicType(type);
  let offset = 8;
  const bounds = [];

  for (let i = 0; i < count; i += 1) {
    const parsed = isVoronoi
      ? parseVoronoiShapeBoundsRaw(raw, offset)
      : parseGridShapeBoundsRaw(raw, offset);
    if (!parsed) {
      return [];
    }
    bounds.push(parsed.bound);
    offset = parsed.nextOffset;
  }

  if (offset !== raw.length) {
    return [];
  }
  return bounds;
}

function readVarU32(raw, offset) {
  let value = 0;
  let shift = 0;
  let cursor = offset;
  while (cursor < raw.length) {
    const byte = raw[cursor];
    cursor += 1;
    value |= (byte & 0x7f) << shift;
    if ((byte & 0x80) === 0) {
      return { value: value >>> 0, nextOffset: cursor };
    }
    shift += 7;
    if (shift > 35) {
      return null;
    }
  }
  return null;
}

function parseSnapshotEntityRows(rawPayload, ownerId, boundId) {
  const raw = Buffer.isBuffer(rawPayload) ? rawPayload : Buffer.from(String(rawPayload || ''), 'utf8');
  if (!raw || raw.length === 0) {
    return [];
  }

  const rows = [];
  let offset = 0;
  while (offset < raw.length) {
    if (offset + 87 > raw.length) {
      break;
    }
    try {
      const entityId = formatUuid(raw.subarray(offset, offset + 16));
      offset += 16;
      const world = raw.readUInt16BE(offset);
      offset += 2;
      const x = raw.readFloatBE(offset);
      const y = raw.readFloatBE(offset + 4);
      const z = raw.readFloatBE(offset + 8);
      offset += 12;

      // Skip transform bounding box min/max.
      offset += 24;

      const isClient = raw.readUInt8(offset) !== 0;
      offset += 1;
      const clientId = formatUuid(raw.subarray(offset, offset + 16));
      offset += 16;

      // Skip PacketSeq + TransferGeneration.
      offset += 16;

      const metadataLenInfo = readVarU32(raw, offset);
      if (!metadataLenInfo) {
        break;
      }
      offset = metadataLenInfo.nextOffset;
      const metadataLen = metadataLenInfo.value;
      if (offset + metadataLen > raw.length) {
        break;
      }
      offset += metadataLen;

      rows.push([
        entityId,
        ownerId || (boundId ? `bound:${boundId}` : 'unknown'),
        String(world),
        String(x),
        String(y),
        String(z),
        isClient ? '1' : '0',
        clientId,
      ]);
    } catch {
      break;
    }
  }

  return rows;
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

function parseNodeManifestPayload(payload) {
  if (payload == null) {
    return null;
  }

  let parsed = null;
  if (Buffer.isBuffer(payload)) {
    try {
      parsed = JSON.parse(payload.toString('utf8'));
    } catch {
      return null;
    }
  } else if (typeof payload === 'string') {
    try {
      parsed = JSON.parse(payload);
    } catch {
      return null;
    }
  } else if (payload && typeof payload === 'object') {
    parsed = payload;
  }

  if (!parsed || typeof parsed !== 'object') {
    return null;
  }

  const nodeName = typeof parsed.nodeName === 'string' ? parsed.nodeName.trim() : '';
  const podName = typeof parsed.podName === 'string' ? parsed.podName.trim() : '';
  const podIp = typeof parsed.podIp === 'string' ? parsed.podIp.trim() : '';
  if (!nodeName && !podName && !podIp) {
    return null;
  }

  return {
    nodeName: nodeName || null,
    podName: podName || null,
    podIp: podIp || null,
  };
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

function toBoundsPolygonShape(bounds, ownerId, color) {
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

  const points = [
    { x: minX, y: minY },
    { x: maxX, y: minY },
    { x: maxX, y: maxY },
    { x: minX, y: maxY },
  ];

  return toPolygonShape(bounds.id, points, ownerId, color);
}

function decodeVoronoiBounds(base64Value) {
  if (typeof base64Value !== 'string' || base64Value.length === 0) {
    return null;
  }
  let raw = null;
  try {
    raw = Buffer.from(base64Value, 'base64');
  } catch {
    return null;
  }
  if (!raw || raw.length < 8) {
    return null;
  }
  try {
    const id = raw.readUInt32BE(0);
    const versionOrCount = raw.readUInt32BE(4);
    if (versionOrCount <= 2) {
      const version = versionOrCount;
      if (version !== 1 || raw.length < 20) {
        return null;
      }

      const site = {
        x: raw.readFloatBE(8),
        y: raw.readFloatBE(12),
      };
      const halfPlaneCount = raw.readUInt32BE(16);
      if (!Number.isFinite(halfPlaneCount) || halfPlaneCount < 0 || halfPlaneCount > 2048) {
        return null;
      }

      let offset = 20;
      const halfPlanes = [];
      for (let i = 0; i < halfPlaneCount; i += 1) {
        if (offset + 12 > raw.length) {
          return null;
        }
        const nx = raw.readFloatBE(offset);
        const ny = raw.readFloatBE(offset + 4);
        const c = raw.readFloatBE(offset + 8);
        offset += 12;
        if (!Number.isFinite(nx) || !Number.isFinite(ny) || !Number.isFinite(c)) {
          return null;
        }
        halfPlanes.push({ nx, ny, c });
      }

      if (offset + 4 > raw.length) {
        return null;
      }
      const count = raw.readUInt32BE(offset);
      offset += 4;
      if (!Number.isFinite(count) || count < 0 || count > 2048) {
        return null;
      }

      const points = [];
      for (let i = 0; i < count; i += 1) {
        if (offset + 8 > raw.length) {
          return null;
        }
        const x = raw.readFloatBE(offset);
        const y = raw.readFloatBE(offset + 4);
        offset += 8;
        if (!Number.isFinite(x) || !Number.isFinite(y)) {
          return null;
        }
        points.push({ x, y });
      }
      return { id, site, halfPlanes, points };
    }

    const count = versionOrCount;
    if (!Number.isFinite(count) || count < 3 || count > 2048) {
      return null;
    }
    let offset = 8;
    const points = [];
    for (let i = 0; i < count; i += 1) {
      if (offset + 8 > raw.length) {
        return null;
      }
      const x = raw.readFloatBE(offset);
      const y = raw.readFloatBE(offset + 4);
      offset += 8;
      if (!Number.isFinite(x) || !Number.isFinite(y)) {
        return null;
      }
      points.push({ x, y });
    }
    return { id, points };
  } catch {
    return null;
  }
}

function normalizeSite(value) {
  if (!value || typeof value !== 'object') {
    return undefined;
  }
  const x = Number(value.x);
  const y = Number(value.y);
  if (!Number.isFinite(x) || !Number.isFinite(y)) {
    return undefined;
  }
  return { x, y };
}

function normalizeHalfPlanes(value) {
  if (!Array.isArray(value)) {
    return undefined;
  }

  const halfPlanes = [];
  for (const plane of value) {
    if (!plane || typeof plane !== 'object') {
      continue;
    }
    const nx = Number(plane.nx ?? plane.normal?.x ?? plane.n?.x);
    const ny = Number(plane.ny ?? plane.normal?.y ?? plane.n?.y);
    const c = Number(plane.c ?? plane.offset ?? plane.constant);
    if (!Number.isFinite(nx) || !Number.isFinite(ny) || !Number.isFinite(c)) {
      continue;
    }
    halfPlanes.push({ nx, ny, c });
  }

  return halfPlanes.length > 0 ? halfPlanes : undefined;
}

function toVoronoiShape(bounds, ownerId, color) {
  if (!bounds || typeof bounds !== 'object') {
    return null;
  }

  if (Array.isArray(bounds.points) && bounds.points.length >= 3) {
    return toPolygonShape(bounds.id, bounds.points, ownerId, color, bounds);
  }

  const site = normalizeSite(bounds.site);
  const halfPlanes = normalizeHalfPlanes(bounds.halfPlanes);
  if (!site || !halfPlanes) {
    return null;
  }

  return {
    id: String(bounds.id),
    ownerId: ownerId || '',
    type: 3,
    position: site,
    radius: 0,
    size: { x: 0, y: 0 },
    color,
    points: [],
    site,
    halfPlanes,
  };
}

function toPolygonShape(id, points, ownerId, color, extras = null) {
  if (!Array.isArray(points) || points.length < 3) {
    return null;
  }
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  let cx = 0;
  let cy = 0;
  for (const p of points) {
    const x = Number(p?.x);
    const y = Number(p?.y);
    if (!Number.isFinite(x) || !Number.isFinite(y)) {
      return null;
    }
    cx += x;
    cy += y;
    minX = Math.min(minX, x);
    minY = Math.min(minY, y);
    maxX = Math.max(maxX, x);
    maxY = Math.max(maxY, y);
  }
  cx /= points.length;
  cy /= points.length;

  // Cartograph expects polygon points to be local offsets from `position`.
  const localPoints = points.map((p) => ({
    x: Number(p.x) - cx,
    y: Number(p.y) - cy,
  }));

  const site = normalizeSite(extras?.site);
  const halfPlanes = normalizeHalfPlanes(extras?.halfPlanes);

  return {
    id: String(id),
    ownerId: ownerId || '',
    type: 3,
    position: { x: cx, y: cy },
    radius: 0,
    size: { x: Math.abs(maxX - minX), y: Math.abs(maxY - minY) },
    color,
    points: localPoints,
    site,
    halfPlanes,
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

function normalizeTransferStage(value) {
  const stage = typeof value === 'string' ? value.trim() : '';
  return VALID_TRANSFER_STAGES.has(stage) ? stage : 'eUnknown';
}

function deriveTransferLinkState(stage) {
  return TRANSFER_STAGE_SOURCE_STATES.has(stage) ? 'source' : 'target';
}

function toTrimmedString(value) {
  return typeof value === 'string' ? value.trim() : '';
}

function decodeIdentityFromBase64(value) {
  const base64 = toTrimmedString(value);
  if (!base64) {
    return '';
  }
  try {
    return decodeNetworkIdentity(Buffer.from(base64, 'base64')).trim();
  } catch {
    return '';
  }
}

function resolveTransferIdentity(record, plainKey, encodedKey) {
  const plain = toTrimmedString(record?.[plainKey]);
  if (plain) {
    return plain;
  }
  return decodeIdentityFromBase64(record?.[encodedKey]);
}

function parseTransferEntityIds(raw) {
  const values = Array.isArray(raw)
    ? raw
    : raw && typeof raw === 'object'
    ? Object.values(raw)
    : [];

  const out = [];
  const seen = new Set();
  for (const value of values) {
    const id = toTrimmedString(value);
    if (!id || seen.has(id)) {
      continue;
    }
    seen.add(id);
    out.push(id);
  }
  return out;
}

async function readHeuristicOwnershipMapFromDatabase(client) {
  const versionRaw =
    typeof client.getBuffer === 'function'
      ? await client.getBuffer(HEURISTIC_OWNERSHIP_VERSION_KEY)
      : await client.get(HEURISTIC_OWNERSHIP_VERSION_KEY);
  const version = normalizeText(versionRaw) || null;

  const fieldKeys =
    typeof client.hkeysBuffer === 'function'
      ? await client.hkeysBuffer(HEURISTIC_OWNERSHIP_NID_TO_BOUND_MAP_KEY)
      : await client.hkeys(HEURISTIC_OWNERSHIP_NID_TO_BOUND_MAP_KEY);

  const ownerByBoundId = {};
  if (Array.isArray(fieldKeys)) {
    for (const rawField of fieldKeys) {
      const rawBoundId =
        typeof client.hgetBuffer === 'function'
          ? await client.hgetBuffer(HEURISTIC_OWNERSHIP_NID_TO_BOUND_MAP_KEY, rawField)
          : await client.hget(
              HEURISTIC_OWNERSHIP_NID_TO_BOUND_MAP_KEY,
              Buffer.isBuffer(rawField) ? rawField : String(rawField)
            );

      const boundId = parseBoundIdText(rawBoundId);
      const ownerId = parseOwnerIdFromOwnershipField(rawField);
      if (!boundId || !ownerId) {
        continue;
      }
      ownerByBoundId[boundId] = ownerId;
    }
  }

  return { version, ownerByBoundId };
}

async function readLegacyHeuristicManifest(client) {
  const payload = await client.call('JSON.GET', HEURISTIC_MANIFEST_KEY, '.');
  return normalizeRedisJsonPayload(payload);
}

async function readAuthorityTelemetryFromDatabase(options = {}) {
  const clientScope = options.dbClientScope || 'authority-telemetry';
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
      if (rows.length > 0) {
        return rows;
      }

      let ownerByBoundId = {};
      try {
        const ownership = await readHeuristicOwnershipMapFromDatabase(client);
        ownerByBoundId = ownership.ownerByBoundId || {};
      } catch {
        ownerByBoundId = {};
      }
      if (Object.keys(ownerByBoundId).length === 0) {
        ownerByBoundId = await readHeuristicClaimedOwnersFromDatabase();
      }
      const boundFieldKeys =
        typeof client.hkeysBuffer === 'function'
          ? await client.hkeysBuffer(SNAPSHOT_BOUNDIDS_TO_ENTITY_LIST_KEY)
          : await client.hkeys(SNAPSHOT_BOUNDIDS_TO_ENTITY_LIST_KEY);

      const snapshotRows = [];
      if (Array.isArray(boundFieldKeys)) {
        for (const rawField of boundFieldKeys) {
          const boundId = parseBoundIdText(rawField);
          if (!boundId) {
            continue;
          }

          const payload =
            typeof client.hgetBuffer === 'function'
              ? await client.hgetBuffer(SNAPSHOT_BOUNDIDS_TO_ENTITY_LIST_KEY, rawField)
              : await client.hget(
                  SNAPSHOT_BOUNDIDS_TO_ENTITY_LIST_KEY,
                  Buffer.isBuffer(rawField) ? rawField : String(rawField)
                );
          if (!payload) {
            continue;
          }

          const ownerId = ownerByBoundId[boundId] || `bound:${boundId}`;
          snapshotRows.push(...parseSnapshotEntityRows(payload, ownerId, boundId));
        }
      }

      snapshotRows.sort((left, right) => left[0].localeCompare(right[0]));
      return snapshotRows;
    }, { clientScope })) || []
  );
}

async function readTransferManifestFromDatabase() {
  return (
    (await withInternalDatabase(async (client) => {
      const payload = await client.call('JSON.GET', TRANSFER_MANIFEST_KEY, '.');
      const manifest = normalizeRedisJsonPayload(payload);
      if (!manifest || typeof manifest !== 'object') {
        return [];
      }

      const transfers = manifest.EntityTransfers;
      if (!transfers || typeof transfers !== 'object') {
        return [];
      }

      const rows = [];
      for (const [transferId, entry] of Object.entries(transfers)) {
        if (!entry || typeof entry !== 'object') {
          continue;
        }

        const stage = normalizeTransferStage(entry.Stage);
        const fromId = resolveTransferIdentity(entry, 'From', 'From(64)');
        const toId = resolveTransferIdentity(entry, 'To', 'To(64)');
        const entityIds = parseTransferEntityIds(entry.EntityIDs);
        if (!fromId || !toId || entityIds.length === 0) {
          continue;
        }

        rows.push({
          transferId: String(transferId).trim() || `${fromId}->${toId}:${stage}`,
          fromId,
          toId,
          stage,
          state: deriveTransferLinkState(stage),
          entityIds,
        });
      }

      rows.sort((left, right) => left.transferId.localeCompare(right.transferId));
      return rows;
    })) || []
  );
}

async function readNetworkTelemetryFromDatabase(options = {}) {
  const includeLiveIds = options.includeLiveIds !== false;
  const clientScope = options.dbClientScope || 'network-telemetry';
  return (
    (await withInternalDatabase(async (client) => {
      const liveShardIds = [];
      if (includeLiveIds) {
        const idsRaw =
          typeof client.hkeysBuffer === 'function'
            ? await client.hkeysBuffer(HEALTH_PING_KEY)
            : await client.hkeys(HEALTH_PING_KEY);
        if (Array.isArray(idsRaw)) {
          for (const rawId of idsRaw) {
            const decoded = decodeNetworkIdentity(rawId).trim();
            if (decoded.length > 0) {
              liveShardIds.push(decoded);
            }
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
    }, { clientScope })) || []
  );
}

async function readShardPlacementFromDatabase() {
  return (
    (await withInternalDatabase(async (client) => {
      const liveIdsRaw =
        typeof client.hkeysBuffer === 'function'
          ? await client.hkeysBuffer(HEALTH_PING_KEY)
          : await client.hkeys(HEALTH_PING_KEY);
      const liveShardSet = new Set();
      if (Array.isArray(liveIdsRaw)) {
        for (const rawId of liveIdsRaw) {
          const shardId = decodeNetworkIdentity(rawId).trim();
          if (shardId.startsWith('eShard ')) {
            liveShardSet.add(shardId);
          }
        }
      }

      const fieldKeys =
        typeof client.hkeysBuffer === 'function'
          ? await client.hkeysBuffer(NODE_MANIFEST_SHARD_NODE_KEY)
          : await client.hkeys(NODE_MANIFEST_SHARD_NODE_KEY);
      if (!Array.isArray(fieldKeys) || fieldKeys.length === 0) {
        return [];
      }

      const rows = [];
      for (const rawField of fieldKeys) {
        const shardId = decodeNetworkIdentity(rawField).trim();
        if (!shardId.startsWith('eShard ') || !liveShardSet.has(shardId)) {
          continue;
        }

        const rawPayload =
          typeof client.hgetBuffer === 'function'
            ? await client.hgetBuffer(NODE_MANIFEST_SHARD_NODE_KEY, rawField)
            : await client.hget(NODE_MANIFEST_SHARD_NODE_KEY, rawField);
        const placement = parseNodeManifestPayload(rawPayload);
        if (!placement) {
          continue;
        }

        rows.push({
          shardId,
          nodeName: placement.nodeName,
          podName: placement.podName,
          podIp: placement.podIp,
        });
      }

      rows.sort((left, right) => left.shardId.localeCompare(right.shardId));
      return rows;
    })) || []
  );
}

async function readHeuristicShapesFromDatabase() {
  return (
    (await withInternalDatabase(async (client) => {
      const heuristicTypeRaw =
        typeof client.getBuffer === 'function'
          ? await client.getBuffer(HEURISTIC_TYPE_KEY)
          : await client.get(HEURISTIC_TYPE_KEY);
      const heuristicType = normalizeHeuristicTypeName(heuristicTypeRaw);

      const heuristicDataRaw =
        typeof client.getBuffer === 'function'
          ? await client.getBuffer(HEURISTIC_DATA_KEY)
          : await client.get(HEURISTIC_DATA_KEY);

      let ownersByBoundId = {};
      try {
        const ownership = await readHeuristicOwnershipMapFromDatabase(client);
        ownersByBoundId = ownership.ownerByBoundId || {};
      } catch {
        ownersByBoundId = {};
      }
      if (Object.keys(ownersByBoundId).length === 0) {
        ownersByBoundId = await readHeuristicClaimedOwnersFromDatabase();
      }
      const decodedBounds = decodeHeuristicBoundsFromBinary(
        Buffer.isBuffer(heuristicDataRaw)
          ? heuristicDataRaw
          : Buffer.from(String(heuristicDataRaw || ''), 'utf8'),
        heuristicType
      );

      const shapes = [];
      if (decodedBounds.length > 0) {
        const isVoronoi = isVoronoiHeuristicType(heuristicType);
        for (const bound of decodedBounds) {
          const boundId = String(bound?.id ?? '').trim();
          if (!boundId) {
            continue;
          }
          const ownerId = ownersByBoundId[boundId] || '';
          const color = ownerId
            ? 'rgba(100, 255, 149, 1)'
            : 'rgba(255, 149, 100, 1)';

          let shape = null;
          if (isVoronoi) {
            shape = toVoronoiShape(bound, ownerId, color);
          } else if (bound.min && bound.max) {
            shape = toBoundsPolygonShape(bound, ownerId, color);
          }
          if (shape) {
            shapes.push(shape);
          }
        }
      }

      if (shapes.length > 0) {
        return shapes;
      }

      const manifest = await readLegacyHeuristicManifest(client);
      if (!manifest) {
        return [];
      }
      const legacyHeuristicType =
        typeof manifest.HeuristicType === 'string'
          ? manifest.HeuristicType.trim()
          : null;

      for (const value of getManifestEntryValues(manifest.Pending)) {
        if (!value || typeof value !== 'object') {
          continue;
        }
        let shape = null;
        if (isVoronoiHeuristicType(legacyHeuristicType)) {
          const decoded = decodeVoronoiBounds(value.BoundsData64);
          if (decoded) {
            shape = toVoronoiShape(decoded, '', 'rgba(255, 149, 100, 1)');
          }
        }
        if (!shape) {
          const bounds = decodeGridShapeBounds(value.BoundsData64);
          if (!bounds) {
            continue;
          }
          shape = toBoundsPolygonShape(bounds, '', 'rgba(255, 149, 100, 1)');
        }
        if (shape) {
          shapes.push(shape);
        }
      }

      for (const value of getManifestEntryValues(manifest.Claimed)) {
        if (!value || typeof value !== 'object') {
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
        let shape = null;
        if (isVoronoiHeuristicType(legacyHeuristicType)) {
          const decoded = decodeVoronoiBounds(value.BoundsData64);
          if (decoded) {
            shape = toVoronoiShape(decoded, ownerId, 'rgba(100, 255, 149, 1)');
          }
        }
        if (!shape) {
          const bounds = decodeGridShapeBounds(value.BoundsData64);
          if (!bounds) {
            continue;
          }
          shape = toBoundsPolygonShape(bounds, ownerId, 'rgba(100, 255, 149, 1)');
        }
        if (shape) {
          shapes.push(shape);
        }
      }

      return shapes;
    })) || []
  );
}

async function readHeuristicClaimedOwnersFromDatabase(options = {}) {
  const clientScope = options.dbClientScope || 'authority-owners';
  const now = Date.now();
  const owners = await withInternalDatabase(async (client) => {
    const { version, ownerByBoundId } =
      await readHeuristicOwnershipMapFromDatabase(client);
    const mapHasEntries = Object.keys(ownerByBoundId).length > 0;

    if (
      heuristicClaimedOwnerCache &&
      heuristicClaimedOwnerCacheVersion &&
      version &&
      heuristicClaimedOwnerCacheVersion === version
    ) {
      return heuristicClaimedOwnerCache;
    }

    if (
      !mapHasEntries &&
      heuristicClaimedOwnerCache &&
      now - heuristicClaimedOwnerCacheAtMs < CLAIMED_OWNER_MAP_CACHE_TTL_MS
    ) {
      return heuristicClaimedOwnerCache;
    }

    if (mapHasEntries) {
      heuristicClaimedOwnerCache = ownerByBoundId;
      heuristicClaimedOwnerCacheAtMs = Date.now();
      heuristicClaimedOwnerCacheVersion = version;
      return ownerByBoundId;
    }

    const manifest = await readLegacyHeuristicManifest(client);
    if (!manifest) {
      return null;
    }

    const legacyOwnerByBoundId = {};
    for (const value of getManifestEntryValues(manifest.Claimed)) {
      if (!value || typeof value !== 'object') {
        continue;
      }

      const boundId = String(value.ID ?? value.id ?? '').trim();
      if (!boundId) {
        continue;
      }

      let ownerId = typeof value.OwnerName === 'string' ? value.OwnerName.trim() : '';
      if (!ownerId && typeof value.Owner64 === 'string') {
        try {
          ownerId = decodeNetworkIdentity(Buffer.from(value.Owner64, 'base64')).trim();
        } catch {
          ownerId = '';
        }
      }

      const normalizedOwnerId = parseOwnerIdFromOwnershipField(ownerId);
      if (normalizedOwnerId) {
        legacyOwnerByBoundId[boundId] = normalizedOwnerId;
      }
    }

    heuristicClaimedOwnerCache = legacyOwnerByBoundId;
    heuristicClaimedOwnerCacheAtMs = Date.now();
    heuristicClaimedOwnerCacheVersion = version;
    return legacyOwnerByBoundId;
  }, { clientScope });

  return owners && typeof owners === 'object'
    ? owners
    : heuristicClaimedOwnerCache || {};
}

async function readHeuristicTypeFromDatabase() {
  return (
    (await withInternalDatabase(async (client) => {
      const rawType =
        typeof client.getBuffer === 'function'
          ? await client.getBuffer(HEURISTIC_TYPE_KEY)
          : await client.get(HEURISTIC_TYPE_KEY);
      const normalizedType = normalizeHeuristicTypeName(rawType);
      if (normalizedType) {
        return normalizedType;
      }

      const payload = await client.call('JSON.GET', HEURISTIC_MANIFEST_KEY, '.HeuristicType');
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
          return normalizeHeuristicTypeName(parsed);
        }
      } catch {}

      const unquoted = raw.replace(/^"(.*)"$/, '$1').trim();
      return normalizeHeuristicTypeName(unquoted);
    })) || null
  );
}

module.exports = {
  readAuthorityTelemetryFromDatabase,
  readTransferManifestFromDatabase,
  readNetworkTelemetryFromDatabase,
  readShardPlacementFromDatabase,
  readHeuristicShapesFromDatabase,
  readHeuristicClaimedOwnersFromDatabase,
  readHeuristicTypeFromDatabase,
};
