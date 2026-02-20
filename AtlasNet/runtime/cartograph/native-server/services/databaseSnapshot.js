const Redis = require('ioredis');
const {
  MAX_PAYLOAD_CHARS,
  MAX_KEYS_PER_DB,
  SCAN_COUNT,
  PROBE_CONNECT_TIMEOUT_MS,
  SNAPSHOT_CONNECT_TIMEOUT_MS,
} = require('../config');
const {
  decodeRedisDisplayValue,
  formatHashPayloadPairs,
  hasHardcodedHashDecoder,
  readHardcodedHashPayload,
  readHardcodedSetPayload,
  shouldDecodeSetKey,
} = require('./databaseReadOnlyView');

function truncatePayload(payload) {
  if (typeof payload !== 'string') {
    return '';
  }
  if (payload.length <= MAX_PAYLOAD_CHARS) {
    return payload;
  }
  return `${payload.slice(0, MAX_PAYLOAD_CHARS)}\n...[truncated ${payload.length - MAX_PAYLOAD_CHARS} bytes]`;
}

function formatSetPayload(values) {
  return [...values].sort((a, b) => String(a).localeCompare(String(b))).join('\n');
}

function formatListPayload(values) {
  return values.map((v, i) => `${i}\t${v}`).join('\n');
}

function formatZSetPayload(valuesWithScores) {
  const lines = [];
  for (let i = 0; i + 1 < valuesWithScores.length; i += 2) {
    lines.push(`${valuesWithScores[i + 1]}\t${valuesWithScores[i]}`);
  }
  return lines.join('\n');
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

function toFiniteNumber(value, fallback) {
  const n = Number(value);
  return Number.isFinite(n) ? n : fallback;
}

function buildRecord(sourceName, key, type, ttlSeconds) {
  return {
    source: sourceName,
    key,
    type,
    entryCount: 0,
    ttlSeconds,
    payload: '',
  };
}

function setRecordPayload(record, payload, entryCount) {
  record.payload = truncatePayload(payload);
  record.entryCount = entryCount;
}

function setRecordReadError(record, err) {
  const message = err instanceof Error ? err.message : 'unknown';
  setRecordPayload(record, `<read error: ${message}>`, 0);
}

async function probeDatabase(target) {
  const startedAt = Date.now();
  const client = createRedisClient(target, PROBE_CONNECT_TIMEOUT_MS);

  try {
    await client.connect();
    const pong = await client.ping();
    return {
      id: target.id,
      name: target.name,
      host: target.host,
      port: target.port,
      running: pong === 'PONG',
      latencyMs: Date.now() - startedAt,
    };
  } catch {
    return {
      id: target.id,
      name: target.name,
      host: target.host,
      port: target.port,
      running: false,
      latencyMs: null,
    };
  } finally {
    try {
      client.disconnect();
    } catch {}
  }
}

async function scanAllKeys(client) {
  const keys = [];
  let cursor = '0';

  do {
    const [nextCursor, page] = await client.scan(cursor, 'COUNT', SCAN_COUNT);
    cursor = String(nextCursor);
    for (const key of page) {
      keys.push(String(key));
      if (keys.length >= MAX_KEYS_PER_DB) {
        return keys.sort((a, b) => a.localeCompare(b));
      }
    }
  } while (cursor !== '0');

  return keys.sort((a, b) => a.localeCompare(b));
}

async function loadKeyMetadata(client, keys) {
  if (keys.length === 0) {
    return [];
  }

  const pipeline = client.pipeline();
  for (const key of keys) {
    pipeline.type(key);
    pipeline.ttl(key);
  }

  const results = await pipeline.exec();
  const metadata = [];
  for (let i = 0; i < keys.length; i += 1) {
    const [typeErr, typeValue] = results[i * 2] || [];
    const [ttlErr, ttlValue] = results[i * 2 + 1] || [];
    metadata.push({
      key: keys[i],
      type: typeErr ? 'unknown' : String(typeValue ?? 'unknown'),
      ttlSeconds: ttlErr ? -2 : toFiniteNumber(ttlValue, -2),
    });
  }

  return metadata;
}

async function applyPipelineByKey(client, keys, buildCommand, onResult) {
  if (keys.length === 0) {
    return;
  }

  const pipeline = client.pipeline();
  for (const key of keys) {
    buildCommand(pipeline, key);
  }

  const results = await pipeline.exec();
  for (let i = 0; i < keys.length; i += 1) {
    const [err, value] = results[i] || [];
    onResult(keys[i], err, value);
  }
}

async function applyPipelineByKeySafely(
  client,
  keys,
  recordsByKey,
  buildCommand,
  onResult
) {
  try {
    await applyPipelineByKey(client, keys, buildCommand, onResult);
  } catch (err) {
    for (const key of keys) {
      const record = recordsByKey.get(key);
      if (record) {
        setRecordReadError(record, err);
      }
    }
  }
}

async function populateHardcodedHashPayloads(
  client,
  keys,
  recordsByKey,
  decodeSerialized
) {
  for (const key of keys) {
    const record = recordsByKey.get(key);
    if (!record) {
      continue;
    }

    try {
      const { payload, entryCount } = await readHardcodedHashPayload(
        client,
        key,
        decodeSerialized
      );
      setRecordPayload(record, payload, entryCount);
    } catch (err) {
      setRecordReadError(record, err);
    }
  }
}

async function populateHardcodedSetPayloads(
  client,
  keys,
  recordsByKey,
  decodeSerialized
) {
  for (const key of keys) {
    const record = recordsByKey.get(key);
    if (!record) {
      continue;
    }

    try {
      const { payload, entryCount } = await readHardcodedSetPayload(
        client,
        key,
        decodeSerialized
      );
      setRecordPayload(record, payload, entryCount);
    } catch (err) {
      setRecordReadError(record, err);
    }
  }
}

async function populateRecordPayloads(client, metadata, recordsByKey, options = {}) {
  const decodeSerialized = options.decodeSerialized !== false;
  const stringKeys = [];
  const hashKeys = [];
  const hardcodedHashKeys = [];
  const setKeys = [];
  const hardcodedSetKeys = [];
  const zsetKeys = [];
  const listKeys = [];
  const jsonFallbackKeys = [];

  for (const item of metadata) {
    const record = recordsByKey.get(item.key);
    if (!record) {
      continue;
    }

    switch (item.type) {
      case 'none':
        setRecordPayload(record, '<key expired>', 0);
        break;
      case 'string':
        stringKeys.push(item.key);
        break;
      case 'hash':
        if (hasHardcodedHashDecoder(item.key)) {
          hardcodedHashKeys.push(item.key);
        } else {
          hashKeys.push(item.key);
        }
        break;
      case 'set':
        if (shouldDecodeSetKey(item.key)) {
          hardcodedSetKeys.push(item.key);
        } else {
          setKeys.push(item.key);
        }
        break;
      case 'zset':
        zsetKeys.push(item.key);
        break;
      case 'list':
        listKeys.push(item.key);
        break;
      case 'stream':
        setRecordPayload(record, '<stream preview not implemented>', 0);
        break;
      case 'vectorset':
        setRecordPayload(record, '<vectorset preview not implemented>', 0);
        break;
      default:
        jsonFallbackKeys.push(item.key);
        break;
    }
  }

  if (stringKeys.length > 0) {
    try {
      const values =
        typeof client.mgetBuffer === 'function'
          ? await client.mgetBuffer(...stringKeys)
          : await client.mget(...stringKeys);

      for (let i = 0; i < stringKeys.length; i += 1) {
        const record = recordsByKey.get(stringKeys[i]);
        if (!record) {
          continue;
        }

        const value = values[i];
        setRecordPayload(
          record,
          value == null ? '' : decodeRedisDisplayValue(value),
          value == null ? 0 : 1
        );
      }
    } catch (err) {
      for (const key of stringKeys) {
        const record = recordsByKey.get(key);
        if (record) {
          setRecordReadError(record, err);
        }
      }
    }
  }

  await applyPipelineByKeySafely(
    client,
    hashKeys,
    recordsByKey,
    (pipeline, key) => pipeline.hgetall(key),
    (key, err, value) => {
      const record = recordsByKey.get(key);
      if (!record) {
        return;
      }
      if (err) {
        setRecordReadError(record, err);
        return;
      }

      const fields = value && typeof value === 'object' ? value : {};
      const pairs = Object.entries(fields).map(([field, fieldValue]) => [
        decodeRedisDisplayValue(field),
        decodeRedisDisplayValue(fieldValue),
      ]);
      setRecordPayload(record, formatHashPayloadPairs(pairs), pairs.length);
    }
  );

  await populateHardcodedHashPayloads(
    client,
    hardcodedHashKeys,
    recordsByKey,
    decodeSerialized
  );

  await applyPipelineByKeySafely(
    client,
    setKeys,
    recordsByKey,
    (pipeline, key) => pipeline.smembers(key),
    (key, err, value) => {
      const record = recordsByKey.get(key);
      if (!record) {
        return;
      }
      if (err) {
        setRecordReadError(record, err);
        return;
      }

      const members = Array.isArray(value) ? value.map(decodeRedisDisplayValue) : [];
      setRecordPayload(record, formatSetPayload(members), members.length);
    }
  );

  await populateHardcodedSetPayloads(
    client,
    hardcodedSetKeys,
    recordsByKey,
    decodeSerialized
  );

  await applyPipelineByKeySafely(
    client,
    zsetKeys,
    recordsByKey,
    (pipeline, key) => pipeline.zrange(key, 0, -1, 'WITHSCORES'),
    (key, err, value) => {
      const record = recordsByKey.get(key);
      if (!record) {
        return;
      }
      if (err) {
        setRecordReadError(record, err);
        return;
      }

      const membersWithScores = Array.isArray(value)
        ? value.map((v, i) => (i % 2 === 0 ? decodeRedisDisplayValue(v) : String(v)))
        : [];
      setRecordPayload(
        record,
        formatZSetPayload(membersWithScores),
        Math.floor(membersWithScores.length / 2)
      );
    }
  );

  await applyPipelineByKeySafely(
    client,
    listKeys,
    recordsByKey,
    (pipeline, key) => pipeline.lrange(key, 0, -1),
    (key, err, value) => {
      const record = recordsByKey.get(key);
      if (!record) {
        return;
      }
      if (err) {
        setRecordReadError(record, err);
        return;
      }

      const members = Array.isArray(value) ? value.map(decodeRedisDisplayValue) : [];
      setRecordPayload(record, formatListPayload(members), members.length);
    }
  );

  await applyPipelineByKeySafely(
    client,
    jsonFallbackKeys,
    recordsByKey,
    (pipeline, key) => pipeline.call('JSON.GET', key, '.'),
    (key, err, value) => {
      const record = recordsByKey.get(key);
      if (!record) {
        return;
      }
      if (err || value == null) {
        setRecordPayload(record, '<unsupported type>', 0);
        return;
      }

      const payload = String(value);
      record.type = 'json';
      setRecordPayload(
        record,
        payload,
        payload === 'null' || payload.length === 0 ? 0 : 1
      );
    }
  );
}

async function readDatabaseRecords(target, options = {}) {
  const client = createRedisClient(target, SNAPSHOT_CONNECT_TIMEOUT_MS);

  try {
    await client.connect();
    const keys = await scanAllKeys(client);
    const metadata = await loadKeyMetadata(client, keys);
    const recordsByKey = new Map();

    for (const item of metadata) {
      recordsByKey.set(
        item.key,
        buildRecord(target.name, item.key, item.type, item.ttlSeconds)
      );
    }

    await populateRecordPayloads(client, metadata, recordsByKey, options);

    return metadata
      .map((item) => recordsByKey.get(item.key))
      .filter((record) => record != null);
  } finally {
    try {
      client.disconnect();
    } catch {}
  }
}

function resolveSelectedSource(runningSources, requestedSource) {
  if (requestedSource) {
    const match = runningSources.find(
      (source) => source.id === requestedSource || source.name === requestedSource
    );
    if (match) {
      return match;
    }
  }

  return runningSources.length > 0 ? runningSources[0] : null;
}

module.exports = {
  probeDatabase,
  readDatabaseRecords,
  resolveSelectedSource,
};
