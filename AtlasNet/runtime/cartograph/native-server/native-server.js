// native-server.js
const express = require('express');
const cors = require('cors');
const Redis = require('ioredis');
const addon = require('../nextjs/native/Web.node');
const { HeuristicDraw, IBoundsDrawShape, std_vector_IBoundsDrawShape_ } = addon; // your .node file

function decodeConnectionRow(row) {
  const hasShardId = row.length >= 14;
  const offset = hasShardId ? 1 : 0;

  return {
    shardId: hasShardId ? row[0] : null,
    IdentityId: row[offset],
    targetId: row[offset + 1],
    pingMs: Number(row[offset + 2]),
    inBytesPerSec: Number(row[offset + 3]),
    outBytesPerSec: Number(row[offset + 4]),
    inPacketsPerSec: Number(row[offset + 5]),
    pendingReliableBytes: Number(row[offset + 6]),
    pendingUnreliableBytes: Number(row[offset + 7]),
    sentUnackedReliableBytes: Number(row[offset + 8]),
    queueTimeUsec: Number(row[offset + 9]),
    qualityLocal: Number(row[offset + 10]),
    qualityRemote: Number(row[offset + 11]),
    state: row[offset + 12],
  };
}
function computeShardAverages(connections) {
  if (!connections || connections.length === 0) {
    return { inAvg: 0, outAvg: 0 };
  }

  let inSum = 0;
  let outSum = 0;

  for (const c of connections) {
    inSum += c.inBytesPerSec;
    outSum += c.outBytesPerSec;
  }

  return {
    inAvg: inSum / connections.length,
    outAvg: outSum / connections.length,
  };
}

const app = express();
app.use(cors()); // allow your frontend to call it

const nt = new addon.NetworkTelemetry();
const authorityTelemetry = addon.AuthorityTelemetry
  ? new addon.AuthorityTelemetry()
  : null;
const dbTargets = [
  {
    id: 'internal',
    name: 'InternalDB',
    host: process.env.INTERNAL_REDIS_SERVICE_NAME || 'InternalDB',
    port: Number(process.env.INTERNAL_REDIS_PORT || 6379),
  },
  {
    id: 'builtin',
    name: 'BuiltInDB_Redis',
    host: process.env.BUILTINDB_REDIS_SERVICE_NAME || 'BuiltInDB_Redis',
    port: Number(process.env.BUILTINDB_REDIS_PORT || 2380),
  },
];

const MAX_PAYLOAD_CHARS = 32 * 1024;
const MAX_KEYS_PER_DB = 2000;
const SCAN_COUNT = 200;

function truncatePayload(payload) {
  if (typeof payload !== 'string') {
    return '';
  }
  if (payload.length <= MAX_PAYLOAD_CHARS) {
    return payload;
  }
  return `${payload.slice(0, MAX_PAYLOAD_CHARS)}\n...[truncated ${payload.length - MAX_PAYLOAD_CHARS} bytes]`;
}

function formatHashPayload(fields) {
  return Object.entries(fields)
    .sort(([a], [b]) => String(a).localeCompare(String(b)))
    .map(([k, v]) => `${k}\t${v}`)
    .join('\n');
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

async function probeDatabase(target) {
  const startedAt = Date.now();
  const client = new Redis({
    host: target.host,
    port: target.port,
    lazyConnect: true,
    connectTimeout: 500,
    maxRetriesPerRequest: 0,
    enableOfflineQueue: false,
    retryStrategy: null,
  });

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

async function readKeyRecord(client, sourceName, key) {
  const type = String(await client.type(key));
  const ttlSeconds = Number(await client.ttl(key));

  let entryCount = 0;
  let payload = '';
  let finalType = type;

  if (type === 'none') {
    payload = '<key expired>';
  } else if (type === 'string') {
    const value = await client.get(key);
    payload = value ?? '';
    entryCount = value == null ? 0 : 1;
  } else if (type === 'hash') {
    const fields = await client.hgetall(key);
    payload = formatHashPayload(fields);
    entryCount = Object.keys(fields).length;
  } else if (type === 'set') {
    const members = await client.smembers(key);
    payload = formatSetPayload(members);
    entryCount = members.length;
  } else if (type === 'zset') {
    const membersWithScores = await client.zrange(key, 0, -1, 'WITHSCORES');
    payload = formatZSetPayload(membersWithScores);
    entryCount = Math.floor(membersWithScores.length / 2);
  } else if (type === 'list') {
    const values = await client.lrange(key, 0, -1);
    payload = formatListPayload(values);
    entryCount = values.length;
  } else if (type === 'stream') {
    payload = '<stream preview not implemented>';
  } else if (type === 'vectorset') {
    payload = '<vectorset preview not implemented>';
  } else {
    try {
      const jsonValue = await client.call('JSON.GET', key, '.');
      if (jsonValue != null) {
        finalType = 'json';
        payload = String(jsonValue);
        entryCount = payload === 'null' || payload.length === 0 ? 0 : 1;
      } else {
        payload = '<unsupported type>';
      }
    } catch {
      payload = '<unsupported type>';
    }
  }

  return {
    source: sourceName,
    key,
    type: finalType,
    entryCount,
    ttlSeconds,
    payload: truncatePayload(payload),
  };
}

async function readDatabaseRecords(target) {
  const client = new Redis({
    host: target.host,
    port: target.port,
    lazyConnect: true,
    connectTimeout: 700,
    maxRetriesPerRequest: 0,
    enableOfflineQueue: false,
    retryStrategy: null,
  });

  try {
    await client.connect();
    const keys = await scanAllKeys(client);
    const records = [];
    for (const key of keys) {
      try {
        const record = await readKeyRecord(client, target.name, key);
        records.push(record);
      } catch (err) {
        records.push({
          source: target.name,
          key,
          type: 'unknown',
          entryCount: 0,
          ttlSeconds: -2,
          payload: `<read error: ${err instanceof Error ? err.message : 'unknown'}>`,
        });
      }
    }
    return records;
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
app.get('/networktelemetry', (req, res) => {
  try {
    const { NetworkTelemetry, std_vector_std_string_, std_vector_std_vector_std_string__ } = addon;

    // SWIG string vector
    const idsVec = new std_vector_std_string_();
    const healthVec = new std_vector_std_string_();
    const telemetryVec = new std_vector_std_vector_std_string__();

    nt.GetLivePingIDs(idsVec, healthVec);
    nt.GetAllTelemetry(telemetryVec);
    // Convert idsVec and healthVec to JS arrays
    const ids = [];
    const healthByShard = new Map();

    const count = Math.min(idsVec.size(), healthVec.size());
    for (let i = 0; i < count; i++) {
    const shardId = String(idsVec.get(i));
    const health = Number(healthVec.get(i)); // ping ms

    ids.push(shardId);
    healthByShard.set(shardId, health);
    }


    // Convert telemetryVec: std::vector<std::vector<std::string>> -> string[][]
    const allRows = [];
    for (let i = 0; i < telemetryVec.size(); i++) {
      const rowVec = telemetryVec.get(i);
      const row = [];
      for (let j = 0; j < rowVec.size(); j++) 
      {
        row.push(String(rowVec.get(j)));
      }
      allRows.push(row);
    }

    /**
     * Heuristic grouping:
     * - If rows include a shardId as first column, group by row[0].
     * - Otherwise: attach the whole dump to each shard (still “displays data”).
     */
    const rowsByShard = new Map();
    for (const row of allRows) {
    if (row.length < 13) {
        continue;
    }

    const decoded = decodeConnectionRow(row);
    const shardId = decoded.shardId ?? decoded.IdentityId;

    if (!rowsByShard.has(shardId)) {
        rowsByShard.set(shardId, []);
    }
    rowsByShard.get(shardId).push(decoded);
    //console.log(`Decoded row for shard ${shardId}:`, decoded);
    }

    // Upload/Download: average from connections
    const telemetry = ids.map((id) => {
    const connections = rowsByShard.get(id) ?? [];
    const { inAvg, outAvg } = computeShardAverages(connections);

    return {
        shardId: id,
        downloadKbps: inAvg,   // avg inBytesPerSec
        uploadKbps: outAvg,   // avg outBytesPerSec
        connections,
    };
    });



    res.json(telemetry);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Native addon failed' });
  }
});

app.get('/authoritytelemetry', (req, res) => {
  try {
    if (!authorityTelemetry || !addon.std_vector_std_vector_std_string__) {
      res.json([]);
      return;
    }

    const telemetryVec = new addon.std_vector_std_vector_std_string__();
    authorityTelemetry.GetAllTelemetry(telemetryVec);

    const rows = [];
    for (let i = 0; i < telemetryVec.size(); i++) {
      const rowVec = telemetryVec.get(i);
      const row = [];
      for (let j = 0; j < rowVec.size(); j++) {
        row.push(String(rowVec.get(j)));
      }
      rows.push(row);
    }

    // Expected row schema:
    // [entityId, ownerId, world, x, y, z, isClient, clientId]
    res.json(rows);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Authority telemetry fetch failed' });
  }
});

app.get('/heuristic', (req, res) => {
    try {
        const hd = new HeuristicDraw();

        // ✅ Create SWIG-compatible vector
        const shapesVector = new std_vector_IBoundsDrawShape_();

        // Call native function
        hd.DrawCurrentHeuristic(shapesVector);

        // Convert to plain JS
        const shapes = [];
        for (let i = 0; i < shapesVector.size(); i++) {
            const shape = shapesVector.get(i);

            // Convert vertices from std_vector_std_pair_float_float__ to array of vec2
            const vertices = [];
            for (let j = 0; j < shape.verticies.size(); j++) {
                const pair = shape.verticies.get(j); // std_pair_float_float_
                vertices.push({
                    x: Number(pair.first),   // convert to JS float
                    y: Number(pair.second),  // convert to JS float
                });
            }


            shapes.push({
                id: shape.id,
                ownerId: shape.owner_id,
                type: shape.type,
                position: { x: shape.pos_x, y: shape.pos_y },
                radius: shape.radius,
                size: { x: shape.size_x, y: shape.size_y },
                color: shape.color,
                vertices, // converted array of {x,y}
            });
        }

        console.log("Successful heuristic fetch");
        res.json(shapes);
    } catch (err) {
        console.error(err);
        res.status(500).json({ error: 'Native addon failed' });
    }
});

app.get('/databases', (req, res) => {
  Promise.all(dbTargets.map(probeDatabase))
    .then(async (probeResults) => {
      const runningSources = probeResults.filter((source) => source.running);
      const requestedSource =
        typeof req.query.source === 'string' ? req.query.source.trim() : '';
      const selectedSource = resolveSelectedSource(runningSources, requestedSource);
      const records = selectedSource ? await readDatabaseRecords(selectedSource) : [];

      res.json({
        sources: runningSources,
        selectedSource: selectedSource ? selectedSource.id : null,
        records,
      });
    })
    .catch((err) => {
      console.error(err);
      res.status(500).json({ error: 'Database snapshot failed' });
    });
});

const PORT = 4000;
app.listen(PORT, '0.0.0.0', () => console.log(`Native server running on port ${PORT}`));
