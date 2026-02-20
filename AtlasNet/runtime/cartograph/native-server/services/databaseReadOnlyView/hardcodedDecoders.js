const { ByteCursor } = require('./byteCursor');
const {
  asBuffer,
  decodeRedisDisplayValue,
  formatBinaryPreview,
  formatUuid,
  formatVec3,
  isLikelyText,
  tryDecodeUtf8,
} = require('./format');

const AUTHORITY_ENTITY_SNAPSHOTS_KEY = 'Authority_EntitySnapshots';
const SERVER_REGISTRY_KEY = 'Server Registry ID_IP';
const SERVER_REGISTRY_PUBLIC_KEY = 'Server Registry ID_IP_public';
const HEALTH_PING_KEY = 'Health_Ping';
const CLIENT_ID_TO_IP_KEY = 'ClientID to IP';
const CLIENT_ID_TO_PROXY_ID_KEY = 'ClientID to ProxyID';
const PROXY_CLIENTS_SET_KEY = 'Proxy_{}_Clients';

const NETWORK_IDENTITY_TYPE_NAME_BY_CODE = {
  0: 'invalid',
  1: 'shard',
  2: 'watchdog',
  3: 'cartograph',
  4: 'gameClient',
  6: 'proxy',
  7: 'atlasNetInitial',
};

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
  } catch (err) {
    const message = err instanceof Error ? err.message : 'unknown';
    return `<decode error: ${message}> ${formatBinaryPreview(value)}`;
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
  } catch (err) {
    const message = err instanceof Error ? err.message : 'unknown';
    return `<decode error: ${message}> ${formatBinaryPreview(value)}`;
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

  const decoded = tryDecodeUtf8(blob);
  if (decoded != null && isLikelyText(decoded)) {
    const compact = decoded.replace(/\s+/g, ' ').trim();
    if (!compact) {
      return `utf8(${blob.length}b)`;
    }
    if (compact.length <= 80) {
      return `utf8:${compact}`;
    }
    return `utf8:${compact.slice(0, 80)}...`;
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
  } catch (err) {
    const message = err instanceof Error ? err.message : 'unknown';
    return `<decode error: ${message}> ${formatBinaryPreview(value)}`;
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
    return decodeRedisDisplayValue(member);
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
  [SERVER_REGISTRY_KEY]: {
    decodeField: (field) => decodeNetworkIdentityValue(field),
    decodeValue: (value) => decodeRedisDisplayValue(value),
  },
  [SERVER_REGISTRY_PUBLIC_KEY]: {
    decodeField: (field) => decodeNetworkIdentityValue(field),
    decodeValue: (value) => decodeRedisDisplayValue(value),
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
    return [decodeRedisDisplayValue(field), decodeRedisDisplayValue(value)];
  }

  return [decoder.decodeField(field), decoder.decodeValue(value)];
}

module.exports = {
  decodeHardcodedHashEntry,
  decodeSetMemberForKey,
  hasHardcodedHashDecoder,
  shouldDecodeSetKey,
};
