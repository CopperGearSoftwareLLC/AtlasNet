
// native-server.js
const express = require('express');
const cors = require('cors');
const { DEFAULT_PORT, getDatabaseTargets } = require('./config');
const {
  COLLECTION_MODE_INTERLINK_HYBRID,
  normalizeCollectionMode,
  resolveCollectionMode,
} = require('./services/collectionMode');
const {
  probeDatabase,
  readDatabaseRecords,
  resolveSelectedSource,
} = require('./services/databaseSnapshot');
const {
  collectNetworkTelemetry,
  collectAuthorityTelemetry,
  collectHeuristicShapes,
} = require('./services/telemetryCollection');
const { readHeuristicTypeFromDatabase } = require('./services/pureDatabaseTelemetry');
const { readWorkersSnapshot } = require('./services/workersSnapshot');

const app = express();
app.use(cors());

let addon = null;
let addonLoadError = null;
try {
  // eslint-disable-next-line global-require
  addon = require('../nextjs/native/Web.node');
} catch (err) {
  addonLoadError = err;
  console.warn(
    '[cartograph] Native addon unavailable; pure_database mode will be used when possible.'
  );
}

let nodeJsWrapper = null;
let networkTelemetry = null;
let entityLedgersView = null;
global.nodeJsWrapper = null;

function ensureHybridCollectors() {
  if (!addon) {
    return {
      addon: null,
      networkTelemetry: null,
      entityLedgersView: null,
    };
  }

  if (!nodeJsWrapper && addon.NodeJSWrapper) {
    nodeJsWrapper = new addon.NodeJSWrapper();
    global.nodeJsWrapper = nodeJsWrapper;
  }
  if (!networkTelemetry && addon.NetworkTelemetry) {
    networkTelemetry = new addon.NetworkTelemetry();
  }
  if (!entityLedgersView && addon.EntityLedgersView) {
    entityLedgersView = new addon.EntityLedgersView();
  }

  return {
    addon,
    networkTelemetry,
    entityLedgersView,
  };
}

function parseBooleanQueryFlag(value, defaultValue) {
  if (value == null) {
    return defaultValue;
  }
  const normalized = String(value).trim().toLowerCase();
  if (normalized.length === 0) {
    return defaultValue;
  }
  if (
    normalized === '0' ||
    normalized === 'false' ||
    normalized === 'off' ||
    normalized === 'no'
  ) {
    return false;
  }
  if (
    normalized === '1' ||
    normalized === 'true' ||
    normalized === 'on' ||
    normalized === 'yes'
  ) {
    return true;
  }
  return defaultValue;
}

function getRequestedCollectionMode(query) {
  if (!query || typeof query !== 'object') {
    return null;
  }

  const rawCollectionMode =
    typeof query.collectionMode === 'string'
      ? query.collectionMode
      : typeof query.mode === 'string'
      ? query.mode
      : '';

  const normalized = normalizeCollectionMode(rawCollectionMode);
  return normalized || null;
}

app.get('/networktelemetry', async (req, res) => {
  try {
    const requestedMode = getRequestedCollectionMode(req.query);
    const mode = resolveCollectionMode(requestedMode);
    const hybridCollectors =
      mode === COLLECTION_MODE_INTERLINK_HYBRID
        ? ensureHybridCollectors()
        : { addon: null, networkTelemetry: null };
    const { modeUsed, data } = await collectNetworkTelemetry({
      addon: hybridCollectors.addon,
      networkTelemetry: hybridCollectors.networkTelemetry,
      requestedMode: mode,
    });
    res.set('x-cartograph-collection-mode', String(modeUsed));
    res.json(data);
  } catch (err) {
    if (addonLoadError) {
      console.error(addonLoadError);
    }
    console.error(err);
    res.status(500).json({ error: 'Network telemetry fetch failed' });
  }
});

app.get('/authoritytelemetry', async (req, res) => {
  try {
    const requestedMode = getRequestedCollectionMode(req.query);
    const mode = resolveCollectionMode(requestedMode);
    const hybridCollectors =
      mode === COLLECTION_MODE_INTERLINK_HYBRID
        ? ensureHybridCollectors()
        : { addon: null, entityLedgersView: null };
    const { modeUsed, data } = await collectAuthorityTelemetry({
      addon: hybridCollectors.addon,
      entityLedgersView: hybridCollectors.entityLedgersView,
      requestedMode: mode,
    });
    res.set('x-cartograph-collection-mode', String(modeUsed));
    res.json(data);
  } catch (err) {
    if (addonLoadError) {
      console.error(addonLoadError);
    }
    console.error(err);
    res.status(500).json({ error: 'Authority telemetry fetch failed' });
  }
});

app.get('/heuristic', async (req, res) => {
  try {
    const requestedMode = getRequestedCollectionMode(req.query);
    const mode = resolveCollectionMode(requestedMode);
    const hybridCollectors =
      mode === COLLECTION_MODE_INTERLINK_HYBRID
        ? ensureHybridCollectors()
        : { addon: null };
    const { modeUsed, data } = await collectHeuristicShapes({
      addon: hybridCollectors.addon,
      requestedMode: mode,
    });
    res.set('x-cartograph-collection-mode', String(modeUsed));
    res.json(data);
  } catch (err) {
    if (addonLoadError) {
      console.error(addonLoadError);
    }
    console.error(err);
    res.status(500).json({ error: 'Heuristic shape fetch failed' });
  }
});

app.get('/heuristictype', async (_req, res) => {
  try {
    const heuristicType = await readHeuristicTypeFromDatabase();
    res.json({ heuristicType: heuristicType || null });
  } catch (err) {
    if (addonLoadError) {
      console.error(addonLoadError);
    }
    console.error(err);
    res.status(500).json({ error: 'Heuristic type fetch failed' });
  }
});

app.get('/databases', async (req, res) => {
  try {
    const probeResults = await Promise.all(
      getDatabaseTargets().map(probeDatabase)
    );
    const runningSources = probeResults.filter((source) => source.running);
    const requestedSource =
      typeof req.query.source === 'string' ? req.query.source.trim() : '';
    const decodeSerialized = parseBooleanQueryFlag(
      req.query.decodeSerialized ?? req.query.decodeEntitySnapshots,
      true
    );
    const selectedSource = resolveSelectedSource(runningSources, requestedSource);
    const records = selectedSource
      ? await readDatabaseRecords(selectedSource, { decodeSerialized })
      : [];

    res.json({
      sources: runningSources,
      selectedSource: selectedSource ? selectedSource.id : null,
      records,
    });
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Database snapshot failed' });
  }
});

app.get('/workers', async (_req, res) => {
  try {
    const snapshot = await readWorkersSnapshot();
    res.json(snapshot);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Workers snapshot failed' });
  }
});

const PORT = Number(process.env.CARTOGRAPH_NATIVE_PORT || DEFAULT_PORT);
app.listen(PORT, '0.0.0.0', () => {
  console.log(`Native server running on port ${PORT}`);
});
