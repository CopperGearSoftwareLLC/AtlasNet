const { ByteCursor } = require('./byteCursor');
const {
  asBuffer,
  decodeRedisDisplayValue,
  decodeRedisRawValue,
  formatBinaryPreview,
  formatUuid,
  formatVec3,
} = require('./format');

const AUTHORITY_ENTITY_SNAPSHOTS_KEY = 'Authority_EntitySnapshots';
const NETWORK_TELEMETRY_KEY = 'Network_Telemetry';
const SERVER_REGISTRY_KEY = 'Server Registry ID_IP';
const SERVER_REGISTRY_PUBLIC_KEY = 'Server Registry ID_IP_public';
const NODE_MANIFEST_SHARD_NODE_KEY = 'Node Manifest Shard_Node';
const SNAPSHOT_BOUNDIDS_TO_ENTITY_LIST_KEY = 'Snapshot:BoundIDs -> EntityList';
const SNAPSHOT_BOUNDIDS_TO_TRANSFORMS_KEY = 'Snapshot:BoundIDs -> Transforms';
const HEALTH_PING_KEY = 'Health_Ping';
const CLIENT_ID_TO_IP_KEY = 'ClientID to IP';
const CLIENT_ID_TO_PROXY_ID_KEY = 'ClientID to ProxyID';
const PROXY_CLIENTS_SET_KEY = 'Proxy_{}_Clients';
const HEURISTIC_MANIFEST_KEY = 'HeuristicManifest';
const HEURISTIC_TYPE_KEY = 'Heuristic:Type';
const HEURISTIC_VERSION_KEY = 'Heuristic:Version';
const HEURISTIC_DATA_KEY = 'Heuristic:Data';
const HEURISTIC_OWNERSHIP_VERSION_KEY = 'Heuristic::Ownership:Version';
const HEURISTIC_OWNERSHIP_NID_TO_BOUND_MAP_KEY =
  'Heuristic::Ownership:NetID -> BoundID Map';
const UUID_PATTERN =
  /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

const NETWORK_IDENTITY_TYPE_NAME_BY_CODE = {
  0: 'invalid',
  1: 'shard',
  2: 'watchdog',
  3: 'cartograph',
  4: 'gameClient',
  6: 'proxy',
  7: 'atlasNetInitial',
};

function isBinaryPreviewText(value) {
  return typeof value === 'string' && value.startsWith('<binary ');
}

function decodeUuidValue(raw) {
  const value = asBuffer(raw);
  if (!value || value.length !== 16) {
    return decodeRedisDisplayValue(raw);
  }
  return formatUuid(value);
}

function decodeNetworkIdentityValue(raw) {
  const value = asBuffer(raw);
  if (!value) {
    return '';
  }

  try {
    const cursor = new ByteCursor(value);
    const typeCode = cursor.readU32();
    const id = formatUuid(cursor.readBytes(16));
    if (cursor.remaining() > 0) {
      return `<decode warning: ${cursor.remaining()} trailing bytes> ${formatBinaryPreview(
        value
      )}`;
    }

    const typeName =
      NETWORK_IDENTITY_TYPE_NAME_BY_CODE[typeCode] ?? `unknown(${typeCode})`;
    return `${typeName}:${id}`;
  } catch {
    return decodeRedisDisplayValue(value);
  }
}

function decodeIpAddressValue(raw) {
  const value = asBuffer(raw);
  if (!value) {
    return '';
  }

  try {
    const cursor = new ByteCursor(value);
    const isIpv4 = cursor.readU8() !== 0;
    const port = cursor.readU16();
    if (!isIpv4) {
      return `ip=<non-ipv4> port=${port}`;
    }

    const ipv4 = cursor.readU32();
    const octets = [
      (ipv4 >>> 24) & 0xff,
      (ipv4 >>> 16) & 0xff,
      (ipv4 >>> 8) & 0xff,
      ipv4 & 0xff,
    ];

    let out = `${octets.join('.')}:${port}`;
    if (cursor.remaining() > 0) {
      out += ` trailingBytes=${cursor.remaining()}`;
    }
    return out;
  } catch {
    return decodeRedisDisplayValue(value);
  }
}

function readVec3(cursor) {
  return {
    x: cursor.readF32(),
    y: cursor.readF32(),
    z: cursor.readF32(),
  };
}

function summarizeBlob(blob) {
  if (!blob || blob.length === 0) {
    return 'empty';
  }

  const decodedValue = decodeRedisDisplayValue(blob);
  if (!isBinaryPreviewText(decodedValue)) {
    const compact = String(decodedValue).replace(/\s+/g, ' ').trim();
    if (!compact) {
      return `utf8(${blob.length}b)`;
    }
    if (compact.length <= 80) {
      return compact;
    }
    return `${compact.slice(0, 80)}...`;
  }

  return formatBinaryPreview(blob);
}

function decodeAuthorityEntitySnapshotValue(raw) {
  const value = asBuffer(raw);
  if (!value) {
    return '';
  }

  try {
    const cursor = new ByteCursor(value);

    const ownerTypeCode = cursor.readU32();
    const ownerId = formatUuid(cursor.readBytes(16));
    const ownerTypeName =
      NETWORK_IDENTITY_TYPE_NAME_BY_CODE[ownerTypeCode] ??
      `unknown(${ownerTypeCode})`;

    const entityId = cursor.readU64().toString();
    const world = cursor.readU16();
    const position = readVec3(cursor);
    const boundsMin = readVec3(cursor);
    const boundsMax = readVec3(cursor);
    const isClient = cursor.readU8() !== 0;
    const clientId = formatUuid(cursor.readBytes(16));
    const metadata = cursor.readBlob();

    let output =
      `owner=${ownerTypeName}:${ownerId} ` +
      `entity=${entityId} world=${world} ` +
      `pos=(${formatVec3(position)}) ` +
      `aabbMin=(${formatVec3(boundsMin)}) ` +
      `aabbMax=(${formatVec3(boundsMax)}) ` +
      `isClient=${isClient ? 1 : 0} ` +
      `clientId=${clientId} ` +
      `metadata=${summarizeBlob(metadata)}`;

    if (cursor.remaining() > 0) {
      output += ` trailingBytes=${cursor.remaining()}`;
    }
    return output;
  } catch {
    return decodeRedisDisplayValue(value);
  }
}

function normalizeText(raw) {
  return decodeRedisDisplayValue(raw).replace(/\0/g, '').trim();
}

function parseBoundIdText(raw) {
  const text = normalizeText(raw);
  const match = text.match(/^\d+/);
  if (!match) {
    return '';
  }
  return String(Number(match[0]));
}

function decodeOwnershipNetIdField(raw) {
  const text = normalizeText(raw);
  if (UUID_PATTERN.test(text)) {
    return `shard:${text}`;
  }
  return decodeNetworkIdentityValue(raw);
}

function readVarString(cursor) {
  const len = cursor.readVarU32();
  const bytes = cursor.readBytes(len);
  return decodeRedisDisplayValue(bytes);
}

function decodeNetworkTelemetryBinaryValue(raw) {
  const value = asBuffer(raw);
  if (!value) {
    return '';
  }

  const utf8Decoded = decodeRedisDisplayValue(value);
  if (typeof utf8Decoded === 'string' && utf8Decoded.includes('\t')) {
    return utf8Decoded;
  }

  try {
    const cursor = new ByteCursor(value);
    const count = cursor.readU32();
    if (!Number.isFinite(count) || count < 0 || count > 10000) {
      return decodeRedisDisplayValue(value);
    }

    const rows = [];
    for (let i = 0; i < count; i += 1) {
      const identityId = readVarString(cursor);
      const targetId = readVarString(cursor);
      const pingMs = cursor.readU32() | 0;
      const inBytesPerSec = cursor.readF32();
      const outBytesPerSec = cursor.readF32();
      const inPacketsPerSec = cursor.readF32();
      const pendingReliableBytes = cursor.readU32();
      const pendingUnreliableBytes = cursor.readU32();
      const sentUnackedReliableBytes = cursor.readU32();
      const queueTimeUsec = Number(cursor.readU64());
      const qualityLocal = cursor.readF32();
      const qualityRemote = cursor.readF32();
      const state = cursor.readU32() | 0;

      rows.push(
        [
          identityId,
          targetId,
          pingMs,
          inBytesPerSec,
          outBytesPerSec,
          inPacketsPerSec,
          pendingReliableBytes,
          pendingUnreliableBytes,
          sentUnackedReliableBytes,
          queueTimeUsec,
          qualityLocal,
          qualityRemote,
          state,
        ].join('\t')
      );
    }

    let out = rows.join('\n');
    if (cursor.remaining() > 0) {
      out += `\n<trailing-bytes ${cursor.remaining()}>`;
    }
    return out;
  } catch {
    return decodeRedisDisplayValue(value);
  }
}

function decodeSnapshotTransformsValue(raw) {
  const value = asBuffer(raw);
  if (!value) {
    return '';
  }
  if (value.length === 0) {
    return '[]';
  }
  // Transform: world(u16) + position(vec3) + aabb(min+max vec3) = 38 bytes.
  if (value.length % 38 !== 0) {
    return decodeRedisDisplayValue(value);
  }

  try {
    const cursor = new ByteCursor(value);
    const rows = [];
    while (cursor.remaining() >= 38) {
      const world = cursor.readU16();
      const position = {
        x: cursor.readF32(),
        y: cursor.readF32(),
        z: cursor.readF32(),
      };
      const min = { x: cursor.readF32(), y: cursor.readF32(), z: cursor.readF32() };
      const max = { x: cursor.readF32(), y: cursor.readF32(), z: cursor.readF32() };
      rows.push({ world, position, aabb: { min, max } });
    }
    return JSON.stringify(rows, null, 2);
  } catch {
    return decodeRedisDisplayValue(value);
  }
}

function decodeSnapshotEntityListValue(raw) {
  const value = asBuffer(raw);
  if (!value) {
    return '';
  }
  if (value.length === 0) {
    return '[]';
  }

  try {
    const cursor = new ByteCursor(value);
    const rows = [];
    while (cursor.remaining() > 0) {
      if (cursor.remaining() < 87) {
        break;
      }
      const entityId = formatUuid(cursor.readBytes(16));
      const world = cursor.readU16();
      const position = {
        x: cursor.readF32(),
        y: cursor.readF32(),
        z: cursor.readF32(),
      };
      const min = { x: cursor.readF32(), y: cursor.readF32(), z: cursor.readF32() };
      const max = { x: cursor.readF32(), y: cursor.readF32(), z: cursor.readF32() };
      const isClient = cursor.readU8() !== 0;
      const clientId = formatUuid(cursor.readBytes(16));
      const packetSeq = Number(cursor.readU64());
      const transferGeneration = Number(cursor.readU64());
      const metadata = cursor.readBlob();

      rows.push({
        entityId,
        world,
        position,
        aabb: { min, max },
        isClient,
        clientId,
        packetSeq,
        transferGeneration,
        metadata: summarizeBlob(metadata),
      });
    }
    return JSON.stringify(rows, null, 2);
  } catch {
    return decodeRedisDisplayValue(value);
  }
}

function shouldDecodeSetKey(key) {
  return (
    key === PROXY_CLIENTS_SET_KEY ||
    (key.startsWith('Proxy_') && key.endsWith('_Clients'))
  );
}

function decodeSetMemberForKey(key, member, decodeEnabled) {
  if (!decodeEnabled) {
    return decodeRedisRawValue(member);
  }
  if (shouldDecodeSetKey(key)) {
    return decodeUuidValue(member);
  }
  return decodeRedisDisplayValue(member);
}

const HARDCODED_HASH_DECODERS = {
  [AUTHORITY_ENTITY_SNAPSHOTS_KEY]: {
    decodeField: (field) => decodeRedisDisplayValue(field),
    decodeValue: (value) => decodeAuthorityEntitySnapshotValue(value),
  },
  [NETWORK_TELEMETRY_KEY]: {
    decodeField: (field) => decodeNetworkIdentityValue(field),
    decodeValue: (value) => decodeNetworkTelemetryBinaryValue(value),
  },
  [SERVER_REGISTRY_KEY]: {
    decodeField: (field) => decodeNetworkIdentityValue(field),
    decodeValue: (value) => decodeRedisDisplayValue(value),
  },
  [SERVER_REGISTRY_PUBLIC_KEY]: {
    decodeField: (field) => decodeNetworkIdentityValue(field),
    decodeValue: (value) => decodeRedisDisplayValue(value),
  },
  [NODE_MANIFEST_SHARD_NODE_KEY]: {
    decodeField: (field) => decodeNetworkIdentityValue(field),
    decodeValue: (value) => decodeRedisDisplayValue(value),
  },
  [HEURISTIC_OWNERSHIP_NID_TO_BOUND_MAP_KEY]: {
    decodeField: (field) => decodeOwnershipNetIdField(field),
    decodeValue: (value) => parseBoundIdText(value),
  },
  [SNAPSHOT_BOUNDIDS_TO_ENTITY_LIST_KEY]: {
    decodeField: (field) => parseBoundIdText(field),
    decodeValue: (value) => decodeSnapshotEntityListValue(value),
  },
  [SNAPSHOT_BOUNDIDS_TO_TRANSFORMS_KEY]: {
    decodeField: (field) => parseBoundIdText(field),
    decodeValue: (value) => decodeSnapshotTransformsValue(value),
  },
  [HEALTH_PING_KEY]: {
    decodeField: (field) => decodeNetworkIdentityValue(field),
    decodeValue: (value) => decodeRedisDisplayValue(value),
  },
  [CLIENT_ID_TO_IP_KEY]: {
    decodeField: (field) => decodeUuidValue(field),
    decodeValue: (value) => decodeIpAddressValue(value),
  },
  [CLIENT_ID_TO_PROXY_ID_KEY]: {
    decodeField: (field) => decodeUuidValue(field),
    decodeValue: (value) => decodeNetworkIdentityValue(value),
  },
};

function hasHardcodedHashDecoder(key) {
  return Object.prototype.hasOwnProperty.call(HARDCODED_HASH_DECODERS, key);
}

function decodeHardcodedHashEntry(key, field, value, decodeEnabled) {
  const decoder = HARDCODED_HASH_DECODERS[key];
  if (!decoder || !decodeEnabled) {
    return [decodeRedisRawValue(field), decodeRedisRawValue(value)];
  }

  return [decoder.decodeField(field), decoder.decodeValue(value)];
}

function decodeBase64BlobSummary(base64Value) {
  if (typeof base64Value !== 'string' || base64Value.length === 0) {
    return decodeRedisDisplayValue(base64Value);
  }
  try {
    const raw = Buffer.from(base64Value, 'base64');
    const decoded = decodeRedisDisplayValue(raw);
    if (!isBinaryPreviewText(decoded)) {
      return decoded;
    }
    return `decodedBytes=${raw.length} preview=${formatBinaryPreview(raw)}`;
  } catch (err) {
    const message = err instanceof Error ? err.message : 'unknown';
    return `<base64 decode error: ${message}>`;
  }
}

function tryDecodeGridShapeBoundsRaw(raw) {
  try {
    const cursor = new ByteCursor(raw);
    const id = cursor.readU32();
    const min = readVec3(cursor);
    const max = readVec3(cursor);
    if (cursor.remaining() !== 0) {
      return null;
    }
    return {
      kind: 'GridShape',
      ID: id,
      aabb: { min, max },
    };
  } catch {
    return null;
  }
}

function tryDecodeVec3Raw(raw) {
  if (raw.length !== 12) {
    return null;
  }
  try {
    const cursor = new ByteCursor(raw);
    return {
      x: cursor.readF32(),
      y: cursor.readF32(),
      z: cursor.readF32(),
    };
  } catch {
    return null;
  }
}

function decodeBoundsDataBase64(base64Value) {
  if (typeof base64Value !== 'string' || base64Value.length === 0) {
    return decodeRedisDisplayValue(base64Value);
  }
  try {
    const raw = Buffer.from(base64Value, 'base64');

    const grid = tryDecodeGridShapeBoundsRaw(raw);
    if (grid) {
      return grid;
    }

    const vec3 = tryDecodeVec3Raw(raw);
    if (vec3) {
      return {
        kind: 'vec3',
        value: vec3,
      };
    }

    const decoded = decodeRedisDisplayValue(raw);
    if (!isBinaryPreviewText(decoded)) {
      return decoded;
    }

    return {
      kind: 'unknown',
      decodedBytes: raw.length,
      preview: formatBinaryPreview(raw),
    };
  } catch (err) {
    const message = err instanceof Error ? err.message : 'unknown';
    return `<base64 decode error: ${message}>`;
  }
}

function decodeHeuristicDataBase64(base64Value) {
  if (typeof base64Value !== 'string' || base64Value.length === 0) {
    return decodeRedisDisplayValue(base64Value);
  }

  try {
    const raw = Buffer.from(base64Value, 'base64');
    const cursor = new ByteCursor(raw);
    const countBig = cursor.readU64();
    const count = Number(countBig);

    const gridShapeSerializedBytes = 4 + 12 + 12;
    if (
      !Number.isFinite(count) ||
      count < 0 ||
      count > 100000 ||
      count * gridShapeSerializedBytes > cursor.remaining()
    ) {
      const decoded = decodeRedisDisplayValue(raw);
      if (!isBinaryPreviewText(decoded)) {
        return decoded;
      }
      return {
        kind: 'unknown',
        decodedBytes: raw.length,
        preview: formatBinaryPreview(raw),
      };
    }

    const bounds = [];
    for (let i = 0; i < count; i += 1) {
      const shapeId = cursor.readU32();
      const min = readVec3(cursor);
      const max = readVec3(cursor);
      bounds.push({
        kind: 'GridShape',
        ID: shapeId,
        aabb: { min, max },
      });
    }

    if (cursor.remaining() !== 0) {
      const decoded = decodeRedisDisplayValue(raw);
      if (!isBinaryPreviewText(decoded)) {
        return decoded;
      }
      return {
        kind: 'unknown',
        decodedBytes: raw.length,
        preview: formatBinaryPreview(raw),
        trailingBytes: cursor.remaining(),
      };
    }

    return {
      kind: 'GridHeuristic',
      boundsCount: count,
      bounds,
    };
  } catch {
    return decodeBase64BlobSummary(base64Value);
  }
}

function decodeBase64NetworkIdentity(base64Value) {
  if (typeof base64Value !== 'string' || base64Value.length === 0) {
    return decodeRedisDisplayValue(base64Value);
  }
  try {
    const raw = Buffer.from(base64Value, 'base64');
    return decodeNetworkIdentityValue(raw);
  } catch (err) {
    const message = err instanceof Error ? err.message : 'unknown';
    return `<base64 decode error: ${message}>`;
  }
}

function tryDecodeGridHeuristicRaw(raw, count) {
  const gridShapeSerializedBytes = 4 + 12 + 12;
  if (
    !Number.isFinite(count) ||
    count < 0 ||
    count > 100000 ||
    count * gridShapeSerializedBytes !== raw.length - 8
  ) {
    return null;
  }

  const cursor = new ByteCursor(raw);
  const _count = Number(cursor.readU64());
  const bounds = [];
  for (let i = 0; i < _count; i += 1) {
    const shapeId = cursor.readU32();
    const min = readVec3(cursor);
    const max = readVec3(cursor);
    bounds.push({
      kind: 'GridShape',
      ID: shapeId,
      aabb: { min, max },
    });
  }
  if (cursor.remaining() !== 0) {
    return null;
  }
  return {
    kind: 'GridHeuristic',
    boundsCount: _count,
    bounds,
  };
}

function tryDecodeVoronoiHeuristicRaw(raw, count) {
  if (!Number.isFinite(count) || count < 0 || count > 100000) {
    return null;
  }

  const cursor = new ByteCursor(raw);
  const _count = Number(cursor.readU64());
  const bounds = [];
  for (let i = 0; i < _count; i += 1) {
    if (cursor.remaining() < 8) {
      return null;
    }
    const shapeId = cursor.readU32();
    const versionOrPointCount = cursor.readU32();

    if (versionOrPointCount <= 2) {
      const version = versionOrPointCount;
      if (version !== 1 || cursor.remaining() < 12) {
        return null;
      }

      const site = {
        x: cursor.readF32(),
        y: cursor.readF32(),
      };
      const halfPlaneCount = cursor.readU32();
      if (!Number.isFinite(halfPlaneCount) || halfPlaneCount < 0 || halfPlaneCount > 8192) {
        return null;
      }
      const halfPlanes = [];
      for (let p = 0; p < halfPlaneCount; p += 1) {
        if (cursor.remaining() < 12) {
          return null;
        }
        halfPlanes.push({
          nx: cursor.readF32(),
          ny: cursor.readF32(),
          c: cursor.readF32(),
        });
      }

      if (cursor.remaining() < 4) {
        return null;
      }
      const pointCount = cursor.readU32();
      if (!Number.isFinite(pointCount) || pointCount < 0 || pointCount > 8192) {
        return null;
      }
      const points = [];
      for (let p = 0; p < pointCount; p += 1) {
        if (cursor.remaining() < 8) {
          return null;
        }
        points.push({
          x: cursor.readF32(),
          y: cursor.readF32(),
        });
      }
      bounds.push({
        kind: 'VoronoiShape',
        ID: shapeId,
        site,
        halfPlanes,
        points,
      });
      continue;
    }

    const pointCount = versionOrPointCount;
    if (!Number.isFinite(pointCount) || pointCount < 3 || pointCount > 8192) {
      return null;
    }
    const points = [];
    for (let p = 0; p < pointCount; p += 1) {
      if (cursor.remaining() < 8) {
        return null;
      }
      points.push({
        x: cursor.readF32(),
        y: cursor.readF32(),
      });
    }
    bounds.push({
      kind: 'VoronoiShape',
      ID: shapeId,
      points,
    });
  }
  if (cursor.remaining() !== 0) {
    return null;
  }
  return {
    kind: 'VoronoiHeuristic',
    boundsCount: _count,
    bounds,
  };
}

function decodeHeuristicDataRawValue(raw, heuristicTypeHint = '') {
  const value = asBuffer(raw);
  if (!value || value.length === 0) {
    return decodeRedisDisplayValue(raw);
  }
  if (value.length < 8) {
    return decodeRedisDisplayValue(value);
  }

  try {
    const count = Number(value.readBigUInt64BE(0));
    const typeHint = String(heuristicTypeHint || '').trim().toLowerCase();
    const preferVoronoi =
      typeHint === 'voronoi' ||
      typeHint === 'hotspotvoronoi' ||
      typeHint === 'llmvoronoi';

    const primary = preferVoronoi
      ? tryDecodeVoronoiHeuristicRaw(value, count)
      : tryDecodeGridHeuristicRaw(value, count);
    if (primary) {
      return primary;
    }

    const secondary = preferVoronoi
      ? tryDecodeGridHeuristicRaw(value, count)
      : tryDecodeVoronoiHeuristicRaw(value, count);
    if (secondary) {
      return secondary;
    }

    return {
      kind: 'unknown',
      decodedBytes: value.length,
      preview: formatBinaryPreview(value),
    };
  } catch {
    return decodeRedisDisplayValue(value);
  }
}

const HARDCODED_STRING_DECODERS = {
  [HEURISTIC_TYPE_KEY]: (value) => normalizeText(value),
  [HEURISTIC_VERSION_KEY]: (value) => normalizeText(value),
  [HEURISTIC_OWNERSHIP_VERSION_KEY]: (value) => normalizeText(value),
  [HEURISTIC_DATA_KEY]: (value) => JSON.stringify(decodeHeuristicDataRawValue(value), null, 2),
};

function hasHardcodedStringDecoder(key) {
  return Object.prototype.hasOwnProperty.call(HARDCODED_STRING_DECODERS, key);
}

function decodeHardcodedStringValue(key, value, decodeEnabled) {
  if (!decodeEnabled) {
    return decodeRedisRawValue(value);
  }
  const decoder = HARDCODED_STRING_DECODERS[key];
  if (!decoder) {
    return decodeRedisDisplayValue(value);
  }
  return decoder(value);
}

function decodeHeuristicManifestJsonPayload(payload, decodeEnabled) {
  if (!decodeEnabled || typeof payload !== 'string' || payload.trim().length === 0) {
    return payload;
  }

  try {
    const parsed = JSON.parse(payload);
    if (!parsed || typeof parsed !== 'object') {
      return payload;
    }

    if (typeof parsed.HeuristicData64 === 'string') {
      parsed.HeuristicData64 = decodeHeuristicDataBase64(parsed.HeuristicData64);
    }

    if (parsed.Pending && typeof parsed.Pending === 'object') {
      for (const item of Object.values(parsed.Pending)) {
        if (!item || typeof item !== 'object') {
          continue;
        }
        if (typeof item.BoundsData64 === 'string') {
          item.BoundsData64 = decodeBoundsDataBase64(item.BoundsData64);
        }
      }
    }

    if (parsed.Claimed && typeof parsed.Claimed === 'object') {
      for (const item of Object.values(parsed.Claimed)) {
        if (!item || typeof item !== 'object') {
          continue;
        }
        if (typeof item.Owner64 === 'string') {
          item.Owner64 = decodeBase64NetworkIdentity(item.Owner64);
        }
        if (typeof item.BoundsData64 === 'string') {
          item.BoundsData64 = decodeBoundsDataBase64(item.BoundsData64);
        }
      }
    }

    return JSON.stringify(parsed, null, 2);
  } catch {
    return payload;
  }
}

function decodeJsonPayloadForKey(key, payload, decodeEnabled) {
  if (key === HEURISTIC_MANIFEST_KEY) {
    return decodeHeuristicManifestJsonPayload(payload, decodeEnabled);
  }
  return payload;
}

module.exports = {
  decodeHardcodedStringValue,
  decodeHardcodedHashEntry,
  decodeJsonPayloadForKey,
  decodeSetMemberForKey,
  hasHardcodedHashDecoder,
  hasHardcodedStringDecoder,
  shouldDecodeSetKey,
};
