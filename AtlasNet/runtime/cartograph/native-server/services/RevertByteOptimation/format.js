const { TextDecoder } = require('util');

const UTF8_DECODER = new TextDecoder('utf-8', { fatal: true });
const MAX_BINARY_PREVIEW_BYTES = 64;
const NETWORK_IDENTITY_TYPE_NAME_BY_CODE = {
  0: 'invalid',
  1: 'shard',
  2: 'watchdog',
  3: 'cartograph',
  4: 'gameClient',
  6: 'proxy',
  7: 'atlasNetInitial',
};

function asBuffer(value) {
  if (value == null) {
    return null;
  }
  if (Buffer.isBuffer(value)) {
    return value;
  }
  if (typeof value === 'string') {
    return Buffer.from(value, 'utf8');
  }
  if (ArrayBuffer.isView(value)) {
    return Buffer.from(value.buffer, value.byteOffset, value.byteLength);
  }
  if (value instanceof ArrayBuffer) {
    return Buffer.from(value);
  }
  return Buffer.from(String(value), 'utf8');
}

function tryDecodeUtf8(buffer) {
  try {
    return UTF8_DECODER.decode(buffer);
  } catch {
    return null;
  }
}

function isLikelyText(value) {
  if (value.length === 0) {
    return true;
  }

  let controlCount = 0;
  for (let i = 0; i < value.length; i += 1) {
    const code = value.charCodeAt(i);
    if (code === 9 || code === 10 || code === 13) {
      continue;
    }
    if (code < 32 || (code >= 127 && code <= 159)) {
      controlCount += 1;
    }
  }
  return controlCount / value.length < 0.03;
}

function formatBinaryPreview(buffer) {
  const preview = buffer.subarray(0, MAX_BINARY_PREVIEW_BYTES);
  const hex = preview.toString('hex').replace(/(..)/g, '$1 ').trim();
  const suffix = buffer.length > preview.length ? ' ...' : '';
  return `<binary ${buffer.length} bytes> ${hex}${suffix}`;
}

function decodeRedisRawValue(value) {
  if (value == null) {
    return '';
  }
  if (typeof value === 'string') {
    return value;
  }

  const buffer = asBuffer(value);
  if (!buffer) {
    return String(value);
  }
  return buffer.toString('utf8');
}

function tryDecodeUtf8HumanText(buffer) {
  const decoded = tryDecodeUtf8(buffer);
  if (decoded == null) {
    return null;
  }
  if (isLikelyText(decoded)) {
    return decoded;
  }

  const stripped = decoded.replace(/\0+/g, '').trim();
  if (stripped.length > 0 && isLikelyText(stripped)) {
    return stripped;
  }
  return null;
}

function tryDecodeUtf16HumanText(buffer) {
  if (buffer.length < 2 || buffer.length % 2 !== 0) {
    return null;
  }
  try {
    const decoded = buffer.toString('utf16le');
    if (decoded.length === 0) {
      return null;
    }
    const stripped = decoded.replace(/\0+/g, '').trim();
    if (stripped.length > 0 && isLikelyText(stripped)) {
      return stripped;
    }
    return null;
  } catch {
    return null;
  }
}

function tryDecodeVec2(buffer) {
  if (buffer.length !== 8) {
    return null;
  }
  return {
    x: buffer.readFloatBE(0),
    y: buffer.readFloatBE(4),
  };
}

function tryDecodeVec3(buffer) {
  if (buffer.length !== 12) {
    return null;
  }
  return {
    x: buffer.readFloatBE(0),
    y: buffer.readFloatBE(4),
    z: buffer.readFloatBE(8),
  };
}

function tryDecodeVec4(buffer) {
  if (buffer.length !== 16) {
    return null;
  }
  return {
    x: buffer.readFloatBE(0),
    y: buffer.readFloatBE(4),
    z: buffer.readFloatBE(8),
    w: buffer.readFloatBE(12),
  };
}

function tryDecodeAabb3f(buffer) {
  if (buffer.length !== 24) {
    return null;
  }
  return {
    min: {
      x: buffer.readFloatBE(0),
      y: buffer.readFloatBE(4),
      z: buffer.readFloatBE(8),
    },
    max: {
      x: buffer.readFloatBE(12),
      y: buffer.readFloatBE(16),
      z: buffer.readFloatBE(20),
    },
  };
}

function tryDecodeUuid(buffer) {
  if (buffer.length !== 16) {
    return null;
  }
  return formatUuid(buffer);
}

function tryDecodeNetworkIdentity(buffer) {
  if (buffer.length !== 20) {
    return null;
  }
  const typeCode = buffer.readUInt32BE(0);
  const typeName =
    NETWORK_IDENTITY_TYPE_NAME_BY_CODE[typeCode] ?? `unknown(${typeCode})`;
  const id = formatUuid(buffer.subarray(4, 20));
  return `${typeName}:${id}`;
}

function tryDecodeIpAddress(buffer) {
  if (buffer.length !== 7) {
    return null;
  }
  const isIpv4 = buffer.readUInt8(0) !== 0;
  const port = buffer.readUInt16BE(1);
  if (!isIpv4) {
    return `ip=<non-ipv4> port=${port}`;
  }
  const ipv4 = buffer.readUInt32BE(3);
  const octets = [
    (ipv4 >>> 24) & 0xff,
    (ipv4 >>> 16) & 0xff,
    (ipv4 >>> 8) & 0xff,
    ipv4 & 0xff,
  ];
  return `${octets.join('.')}:${port}`;
}

function tryDecodeKnownStructured(buffer) {
  const asNetworkIdentity = tryDecodeNetworkIdentity(buffer);
  if (asNetworkIdentity) {
    return `networkIdentity:${asNetworkIdentity}`;
  }

  const asIp = tryDecodeIpAddress(buffer);
  if (asIp) {
    return `ipAddress:${asIp}`;
  }

  const asUuid = tryDecodeUuid(buffer);
  if (asUuid) {
    return `uuid:${asUuid}`;
  }

  const asAabb = tryDecodeAabb3f(buffer);
  if (asAabb) {
    return `aabb3f:${JSON.stringify(asAabb)}`;
  }

  const asVec4 = tryDecodeVec4(buffer);
  if (asVec4) {
    return `vec4:${JSON.stringify(asVec4)}`;
  }

  const asVec3 = tryDecodeVec3(buffer);
  if (asVec3) {
    return `vec3:${JSON.stringify(asVec3)}`;
  }

  const asVec2 = tryDecodeVec2(buffer);
  if (asVec2) {
    return `vec2:${JSON.stringify(asVec2)}`;
  }

  return null;
}

function decodeRedisDisplayValue(value, options = {}) {
  const interpretTypes = options.interpretTypes !== false;
  if (!interpretTypes) {
    return decodeRedisRawValue(value);
  }

  const rawValue = decodeRedisRawValue(value);
  if (typeof value === 'string') {
    return rawValue;
  }

  const buffer = asBuffer(value);
  if (!buffer || buffer.length === 0) {
    return rawValue;
  }

  const utf8HumanText = tryDecodeUtf8HumanText(buffer);
  if (utf8HumanText != null) {
    return utf8HumanText;
  }

  const utf16HumanText = tryDecodeUtf16HumanText(buffer);
  if (utf16HumanText != null) {
    return utf16HumanText;
  }

  const structured = tryDecodeKnownStructured(buffer);
  if (structured != null) {
    return structured;
  }

  return formatBinaryPreview(buffer);
}

function formatUuid(bytes) {
  const hex = bytes.toString('hex');
  if (hex.length !== 32) {
    return hex;
  }
  return [
    hex.slice(0, 8),
    hex.slice(8, 12),
    hex.slice(12, 16),
    hex.slice(16, 20),
    hex.slice(20),
  ].join('-');
}

function formatNumber(value) {
  if (!Number.isFinite(value)) {
    return String(value);
  }
  if (Math.abs(value) >= 1000 || (Math.abs(value) > 0 && Math.abs(value) < 0.001)) {
    return value.toExponential(3);
  }
  return value.toFixed(3).replace(/\.?0+$/, '');
}

function formatVec3(value) {
  return `${formatNumber(value.x)},${formatNumber(value.y)},${formatNumber(value.z)}`;
}

module.exports = {
  asBuffer,
  decodeRedisDisplayValue,
  decodeRedisRawValue,
  formatBinaryPreview,
  formatNumber,
  formatUuid,
  formatVec3,
  isLikelyText,
  tryDecodeAabb3f,
  tryDecodeIpAddress,
  tryDecodeKnownStructured,
  tryDecodeNetworkIdentity,
  tryDecodeUtf16HumanText,
  tryDecodeUtf8HumanText,
  tryDecodeUuid,
  tryDecodeVec2,
  tryDecodeVec3,
  tryDecodeVec4,
  tryDecodeUtf8,
};
