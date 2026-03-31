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
    return { inAvg: 0, outAvg: 0, pingAvg: null };
  }

  let inSum = 0;
  let outSum = 0;
  let pingSum = 0;
  let pingCount = 0;

  for (const c of connections) {
    inSum += c.inBytesPerSec;
    outSum += c.outBytesPerSec;
    if (Number.isFinite(c.pingMs) && c.pingMs >= 0) {
      pingSum += c.pingMs;
      pingCount += 1;
    }
  }

  return {
    inAvg: inSum / connections.length,
    outAvg: outSum / connections.length,
    pingAvg: pingCount > 0 ? pingSum / pingCount : null,
  };
}

function normalizeAvgPingMs(value) {
  const pingMs = Number(value);
  if (!Number.isFinite(pingMs) || pingMs < 0) {
    return null;
  }
  return pingMs;
}

function toStringRows(telemetryVec) {
  const rows = [];
  for (let i = 0; i < telemetryVec.size(); i += 1) {
    const rowVec = telemetryVec.get(i);
    const row = [];
    for (let j = 0; j < rowVec.size(); j += 1) {
      row.push(String(rowVec.get(j)));
    }
    rows.push(row);
  }
  return rows;
}

function buildNetworkTelemetry(ids, rows, options = {}) {
  const includeLiveIds = options.includeLiveIds !== false;
  const normalizedIds = [];
  if (Array.isArray(ids)) {
    for (const id of ids) {
      const shardId = String(id ?? '').trim();
      if (shardId.length > 0) {
        normalizedIds.push(shardId);
      }
    }
  }
  const liveIdSet = new Set(normalizedIds);

  const rowsByShard = new Map();
  if (Array.isArray(rows)) {
    for (const row of rows) {
      if (!Array.isArray(row) || row.length < 13) {
        continue;
      }

      const decoded = decodeConnectionRow(row);
      const shardId = String(decoded.shardId ?? decoded.IdentityId ?? '').trim();
      if (shardId.length === 0) {
        continue;
      }
      if (includeLiveIds && liveIdSet.size > 0 && !liveIdSet.has(shardId)) {
        continue;
      }
      if (!rowsByShard.has(shardId)) {
        rowsByShard.set(shardId, []);
      }
      rowsByShard.get(shardId).push(decoded);
    }
  }

  const orderedShardIds = [];
  const seen = new Set();
  for (const id of normalizedIds) {
    if (seen.has(id)) {
      continue;
    }
    seen.add(id);
    orderedShardIds.push(id);
  }
  if (!includeLiveIds || liveIdSet.size === 0) {
    for (const shardId of Array.from(rowsByShard.keys()).sort()) {
      if (seen.has(shardId)) {
        continue;
      }
      seen.add(shardId);
      orderedShardIds.push(shardId);
    }
  }

  return orderedShardIds.map((id) => {
    const connections = (rowsByShard.get(id) ?? []).filter((connection) => {
      if (!includeLiveIds || liveIdSet.size === 0) {
        return true;
      }
      const targetId = String(connection.targetId ?? '').trim();
      return liveIdSet.has(targetId);
    });
    const { inAvg, outAvg, pingAvg } = computeShardAverages(connections);
    return {
      shardId: id,
      downloadKbps: inAvg,
      uploadKbps: outAvg,
      avgPingMs: pingAvg,
      connections,
    };
  });
}

function readNetworkTelemetry(addon, networkTelemetry, options = {}) {
  if (
    !addon ||
    !networkTelemetry ||
    !addon.std_vector_std_string_ ||
    !addon.std_vector_std_vector_std_string__
  ) {
    return [];
  }
  const includeLiveIds = options.includeLiveIds !== false;

  const {
    std_vector_std_string_,
    std_vector_std_vector_std_string__,
  } = addon;

  const idsVec = new std_vector_std_string_();
  const healthVec = new std_vector_std_string_();
  const telemetryVec = new std_vector_std_vector_std_string__();

  if (includeLiveIds) {
    networkTelemetry.GetLivePingIDs(idsVec, healthVec);
  }
  networkTelemetry.GetAllTelemetry(telemetryVec);

  const ids = [];
  const count = Math.min(idsVec.size(), healthVec.size());
  for (let i = 0; i < count; i += 1) {
    ids.push(String(idsVec.get(i)));
  }

  return buildNetworkTelemetry(ids, toStringRows(telemetryVec), { includeLiveIds });
}

module.exports = {
  buildNetworkTelemetry,
  normalizeAvgPingMs,
  readNetworkTelemetry,
};
