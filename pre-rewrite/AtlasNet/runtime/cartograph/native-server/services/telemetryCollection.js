const { readNetworkTelemetry } = require('./networkTelemetry');
const { readEntityLedgersTelemetry } = require('./entityLedgerTelemetry');
const { readHeuristicShapes } = require('./heuristicShapes');
const {
  readHeuristicClaimedOwnersFromDatabase,
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

const authorityOwnerMapCache = createTelemetryCache({});

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
  const hasAddon = Boolean(
    addon &&
      networkTelemetry &&
      addon.std_vector_std_string_ &&
      addon.std_vector_std_vector_std_string__
  );
  if (!hasAddon) {
    return [];
  }

  const addonRead = await runTelemetryRead(
    'network GNS interlink',
    () => readNetworkTelemetry(addon, networkTelemetry, { includeLiveIds }),
    []
  );
  if (addonRead.ok) {
    return asArray(addonRead.value);
  }
  throw addonRead.error || new Error('Network telemetry failed');
}

async function collectAuthorityTelemetry({ addon, entityLedgersView }) {
  const hasAddon = Boolean(
    addon && entityLedgersView && addon.std_vector_StreamEntityLedgerEntry_
  );
  const ownerMapReadPromise = scheduleCachedTelemetryRead(
    authorityOwnerMapCache,
    'authority ownership map',
    () => readHeuristicClaimedOwnersFromDatabase({ dbClientScope: 'authority-owners' }),
    {},
    500
  );

  if (!hasAddon) {
    return [];
  }

  const addonRead = await runTelemetryRead(
    'authority GNS interlink',
    () =>
      readEntityLedgersTelemetry(addon, entityLedgersView, {
        ownerByBoundId: getCachedTelemetryObject(authorityOwnerMapCache),
      }),
    []
  );
  if (addonRead.ok) {
    return asArray(addonRead.value);
  }

  await ownerMapReadPromise;
  throw addonRead.error || new Error('Authority telemetry failed');
}

async function collectHeuristicShapes({ addon }) {
  const databaseRead = await runTelemetryRead(
    'heuristic shapes internalDB',
    () => readHeuristicShapesFromDatabase(),
    []
  );
  if (databaseRead.ok && asArray(databaseRead.value).length > 0) {
    return asArray(databaseRead.value);
  }

  const hasAddon = Boolean(
    addon && addon.HeuristicDraw && addon.std_vector_IBoundsDrawShape_
  );
  if (hasAddon) {
    return readHeuristicShapes(addon);
  }
  return [];
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
