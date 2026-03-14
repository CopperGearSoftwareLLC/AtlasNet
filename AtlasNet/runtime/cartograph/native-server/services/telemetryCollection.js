const { normalizeAvgPingMs, readNetworkTelemetry } = require('./networkTelemetry');
const { readEntityLedgersTelemetry } = require('./entityLedgerTelemetry');
const { readHeuristicShapes } = require('./heuristicShapes');
const {
  readAuthorityTelemetryFromDatabase,
  readHeuristicClaimedOwnersFromDatabase,
  readNetworkTelemetryFromDatabase,
  readHeuristicShapesFromDatabase,
  readTransferManifestFromDatabase,
} = require('./databaseTelemetry');

function asArray(value) {
  return Array.isArray(value) ? value : [];
}

function createTelemetryCache(initialValue) {
  return {
    value: initialValue,
    inFlight: null,
    lastUpdatedAtMs: 0,
  };
}

const authorityTelemetryDbCache = createTelemetryCache([]);
const authorityOwnerMapCache = createTelemetryCache({});
const networkTelemetryDbCacheByMode = new Map();

async function runTelemetryRead(readLabel, readFn, fallbackValue = []) {
  try {
    return {
      ok: true,
      value: await readFn(),
      error: null,
    };
  } catch (error) {
    console.warn(`[cartograph] ${readLabel} read failed`);
    console.warn(error);
    return {
      ok: false,
      value: fallbackValue,
      error,
    };
  }
}

function getCachedTelemetrySnapshot(cache) {
  return Array.isArray(cache.value) ? cache.value : [];
}

function getCachedTelemetryObject(cache) {
  return cache.value && typeof cache.value === 'object' ? cache.value : {};
}

function scheduleCachedTelemetryRead(
  cache,
  readLabel,
  readFn,
  fallbackValue = [],
  staleAfterMs = 0
) {
  if (
    staleAfterMs > 0 &&
    cache.lastUpdatedAtMs > 0 &&
    Date.now() - cache.lastUpdatedAtMs < staleAfterMs
  ) {
    return Promise.resolve({
      ok: true,
      value: cache.value,
      error: null,
    });
  }

  if (cache.inFlight) {
    return cache.inFlight;
  }

  cache.inFlight = runTelemetryRead(readLabel, readFn, fallbackValue).then((result) => {
    if (result.ok) {
      cache.value = result.value;
      cache.lastUpdatedAtMs = Date.now();
    }
    cache.inFlight = null;
    return result;
  });

  return cache.inFlight;
}

function mergeNetworkTelemetryRows(primaryRows, secondaryRows) {
  const byShardId = new Map();
  const ingest = (rows, { prefer }) => {
    for (const row of asArray(rows)) {
      const shardId = String(row?.shardId ?? '').trim();
      if (!shardId) {
        continue;
      }

      const previous = byShardId.get(shardId);
      const rowConnections = asArray(row?.connections);
      const previousConnections = asArray(previous?.connections);
      const hasRowConnections = rowConnections.length > 0;
      const hasPreviousConnections = previousConnections.length > 0;
      const rowAvgPingMs = normalizeAvgPingMs(row?.avgPingMs);
      const previousAvgPingMs = normalizeAvgPingMs(previous?.avgPingMs);

      const next = {
        shardId,
        downloadKbps: Number.isFinite(Number(row?.downloadKbps))
          ? Number(row.downloadKbps)
          : Number(previous?.downloadKbps) || 0,
        uploadKbps: Number.isFinite(Number(row?.uploadKbps))
          ? Number(row.uploadKbps)
          : Number(previous?.uploadKbps) || 0,
        avgPingMs: rowAvgPingMs ?? previousAvgPingMs,
        connections:
          hasRowConnections || !hasPreviousConnections
            ? rowConnections
            : previousConnections,
      };

      if (!previous || prefer) {
        byShardId.set(shardId, next);
      } else {
        byShardId.set(shardId, {
          ...previous,
          ...next,
          avgPingMs: previousAvgPingMs ?? next.avgPingMs,
          connections:
            hasPreviousConnections && !hasRowConnections
              ? previousConnections
              : next.connections,
        });
      }
    }
  };

  ingest(secondaryRows, { prefer: false });
  ingest(primaryRows, { prefer: true });
  return Array.from(byShardId.values());
}

function mergeAuthorityTelemetryRows(primaryRows, secondaryRows) {
  const byEntityId = new Map();
  const ingest = (rows) => {
    for (const row of asArray(rows)) {
      if (!Array.isArray(row) || row.length === 0) {
        continue;
      }
      const entityId = String(row[0] ?? '').trim();
      if (!entityId) {
        continue;
      }
      byEntityId.set(entityId, row.map((value) => String(value ?? '')));
    }
  };

  ingest(secondaryRows);
  ingest(primaryRows);

  return Array.from(byEntityId.values()).sort((left, right) =>
    String(left[0] ?? '').localeCompare(String(right[0] ?? ''))
  );
}

function readTransferStateQueueTelemetry(addon, transferStateQueueView) {
  if (
    !addon ||
    !transferStateQueueView ||
    !addon.std_vector_std_vector_std_string__
  ) {
    return [];
  }

  const telemetryVec = new addon.std_vector_std_vector_std_string__();
  transferStateQueueView.DrainTransferStateQueue(telemetryVec);

  const events = [];
  for (let i = 0; i < telemetryVec.size(); i += 1) {
    const rowVec = telemetryVec.get(i);
    const row = [];
    for (let j = 0; j < rowVec.size(); j += 1) {
      row.push(String(rowVec.get(j)));
    }

    if (row.length < 6) {
      continue;
    }

    const transferId = String(row[0] || '').trim();
    const fromId = String(row[1] || '').trim();
    const toId = String(row[2] || '').trim();
    const stage = String(row[3] || '').trim() || 'eUnknown';
    const state = String(row[4] || '').trim() || 'source';
    const timestampMs = Number(row[5]);
    const entityIds = row
      .slice(6)
      .map((value) => String(value || '').trim())
      .filter((value) => value.length > 0);

    if (!transferId || !fromId || !toId || entityIds.length === 0) {
      continue;
    }

    events.push({
      transferId,
      fromId,
      toId,
      stage,
      state,
      entityIds,
      timestampMs: Number.isFinite(timestampMs) ? Math.floor(timestampMs) : Date.now(),
    });
  }

  return events;
}

async function collectNetworkTelemetry({
  addon,
  networkTelemetry,
  includeLiveIds = true,
}) {
  const dbCacheKey = includeLiveIds ? 'with-live-ids' : 'without-live-ids';
  const dbCache =
    networkTelemetryDbCacheByMode.get(dbCacheKey) ?? createTelemetryCache([]);
  networkTelemetryDbCacheByMode.set(dbCacheKey, dbCache);
  const hasAddon = Boolean(
    addon &&
      networkTelemetry &&
      addon.std_vector_std_string_ &&
      addon.std_vector_std_vector_std_string__
  );
  const databaseReadPromise = scheduleCachedTelemetryRead(
    dbCache,
    'network internalDB',
    () =>
      readNetworkTelemetryFromDatabase({
        includeLiveIds,
        dbClientScope: 'network-telemetry',
      }),
    [],
    500
  );

  if (!hasAddon) {
    const databaseRead = await databaseReadPromise;
    return asArray(databaseRead.value);
  }

  const addonRead = await runTelemetryRead(
    'network GNS interlink',
    () => readNetworkTelemetry(addon, networkTelemetry, { includeLiveIds }),
    []
  );
  const cachedDatabaseRows = getCachedTelemetrySnapshot(dbCache);
  const mergedRows = mergeNetworkTelemetryRows(
    addonRead.value,
    cachedDatabaseRows
  );
  if (mergedRows.length > 0) {
    return mergedRows;
  }
  if (addonRead.ok) {
    return asArray(addonRead.value);
  }

  const databaseRead = await databaseReadPromise;
  const fallbackMergedRows = mergeNetworkTelemetryRows(
    addonRead.value,
    databaseRead.value
  );
  if (fallbackMergedRows.length > 0) {
    return fallbackMergedRows;
  }
  if (databaseRead.ok) {
    return asArray(databaseRead.value);
  }
  throw addonRead.error || databaseRead.error || new Error('Network telemetry failed');
}

async function collectAuthorityTelemetry({ addon, entityLedgersView }) {
  const hasAddon = Boolean(
    addon && entityLedgersView && addon.std_vector_StreamEntityLedgerEntry_
  );
  const databaseReadPromise = scheduleCachedTelemetryRead(
    authorityTelemetryDbCache,
    'authority internalDB',
    () => readAuthorityTelemetryFromDatabase({ dbClientScope: 'authority-telemetry' }),
    [],
    1000
  );
  const ownerMapReadPromise = scheduleCachedTelemetryRead(
    authorityOwnerMapCache,
    'authority ownership map',
    () => readHeuristicClaimedOwnersFromDatabase({ dbClientScope: 'authority-owners' }),
    {},
    500
  );

  if (!hasAddon) {
    const databaseRead = await databaseReadPromise;
    return asArray(databaseRead.value);
  }

  const addonRead = await runTelemetryRead(
    'authority GNS interlink',
    () =>
      readEntityLedgersTelemetry(addon, entityLedgersView, {
        ownerByBoundId: getCachedTelemetryObject(authorityOwnerMapCache),
      }),
    []
  );
  const cachedDatabaseRows = getCachedTelemetrySnapshot(authorityTelemetryDbCache);
  const mergedRows = mergeAuthorityTelemetryRows(
    addonRead.value,
    cachedDatabaseRows
  );
  if (mergedRows.length > 0) {
    return mergedRows;
  }
  if (addonRead.ok) {
    return asArray(addonRead.value);
  }

  await ownerMapReadPromise;
  const databaseRead = await databaseReadPromise;
  const fallbackMergedRows = mergeAuthorityTelemetryRows(
    addonRead.value,
    databaseRead.value
  );
  if (fallbackMergedRows.length > 0) {
    return fallbackMergedRows;
  }
  if (databaseRead.ok) {
    return asArray(databaseRead.value);
  }
  throw addonRead.error || databaseRead.error || new Error('Authority telemetry failed');
}

async function collectHeuristicShapes({ addon }) {
  const hasAddon = Boolean(
    addon && addon.HeuristicDraw && addon.std_vector_IBoundsDrawShape_
  );
  if (hasAddon) {
    return readHeuristicShapes(addon);
  }
  return readHeuristicShapesFromDatabase();
}

async function collectTransferManifest() {
  return readTransferManifestFromDatabase();
}

async function collectTransferStateQueue({ addon, transferStateQueueView }) {
  const hasAddon = Boolean(
    addon && transferStateQueueView && addon.std_vector_std_vector_std_string__
  );
  if (hasAddon) {
    return readTransferStateQueueTelemetry(addon, transferStateQueueView);
  }
  return [];
}

module.exports = {
  collectNetworkTelemetry,
  collectAuthorityTelemetry,
  collectHeuristicShapes,
  collectTransferManifest,
  collectTransferStateQueue,
};
