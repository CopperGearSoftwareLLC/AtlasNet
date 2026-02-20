const { TextDecoder } = require('util');

const UTF8_DECODER = new TextDecoder('utf-8', { fatal: true });
const MAX_BINARY_PREVIEW_BYTES = 64;

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

function decodeRedisDisplayValue(value) {
  if (value == null) {
    return '';
  }
  if (typeof value === 'string') {
    return value;
  }

  const buffer = asBuffer(value);
  if (!buffer || buffer.length === 0) {
    return '';
  }

  const decoded = tryDecodeUtf8(buffer);
  if (decoded != null && isLikelyText(decoded)) {
    return decoded;
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
  formatBinaryPreview,
  formatNumber,
  formatUuid,
  formatVec3,
  isLikelyText,
  tryDecodeUtf8,
};
