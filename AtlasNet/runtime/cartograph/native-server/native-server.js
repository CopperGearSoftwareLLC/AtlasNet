
// native-server.js
const express = require('express');
const cors = require('cors');
const { DEFAULT_PORT, getDatabaseTargets } = require('./config');
const {
  probeDatabase,
  readDatabaseRecords,
  resolveSelectedSource,
} = require('./services/databaseSnapshot');
const {
  collectNetworkTelemetry,
  collectAuthorityTelemetry,
  collectHeuristicShapes,
  collectTransferManifest,
  collectTransferStateQueue,
} = require('./services/telemetryCollection');
const {
  readHeuristicTypeFromDatabase,
  readShardPlacementFromDatabase,
} = require('./services/databaseTelemetry');
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
    '[cartograph] Native addon unavailable; using database-backed telemetry where applicable.'
  );
}

let nodeJsWrapper = null;
let networkTelemetry = null;
let entityLedgersView = null;
let transferStateQueueView = null;
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
  if (!entityLedgersView && addon.StreamEntityLedgersView) {
    entityLedgersView = new addon.StreamEntityLedgersView();
  }
  if (!transferStateQueueView && addon.TransferStateQueueView) {
    transferStateQueueView = new addon.TransferStateQueueView();
  }

  return {
    addon,
    networkTelemetry,
    entityLedgersView,
    transferStateQueueView,
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

async function respondJson(res, readData, errorMessage, includeAddonError = true) {
  try {
    res.json(await readData());
  } catch (err) {
    if (includeAddonError && addonLoadError) {
      console.error(addonLoadError);
    }
    console.error(err);
    res.status(500).json({ error: errorMessage });
  }
}

app.get('/networktelemetry', async (req, res) => {
  return respondJson(
    res,
    async () => {
      const hybridCollectors = ensureHybridCollectors();
      const includeLiveIds = parseBooleanQueryFlag(req.query.liveIds, true);
      return collectNetworkTelemetry({
        addon: hybridCollectors.addon,
        networkTelemetry: hybridCollectors.networkTelemetry,
        includeLiveIds,
      });
    },
    'Network telemetry fetch failed'
  );
});

app.get('/authoritytelemetry', async (_req, res) => {
  return respondJson(
    res,
    async () => {
      const hybridCollectors = ensureHybridCollectors();
      return collectAuthorityTelemetry({
        addon: hybridCollectors.addon,
        entityLedgersView: hybridCollectors.entityLedgersView,
      });
    },
    'Authority telemetry fetch failed'
  );
});

app.get('/transfermanifest', async (_req, res) => {
  return respondJson(res, collectTransferManifest, 'Transfer manifest fetch failed');
});

app.get('/transferstatequeue', async (_req, res) => {
  return respondJson(
    res,
    async () => {
      const hybridCollectors = ensureHybridCollectors();
      return collectTransferStateQueue({
        addon: hybridCollectors.addon,
        transferStateQueueView: hybridCollectors.transferStateQueueView,
      });
    },
    'Transfer state queue fetch failed'
  );
});

app.get('/heuristic', async (_req, res) => {
  return respondJson(
    res,
    async () => {
      const hybridCollectors = ensureHybridCollectors();
      return collectHeuristicShapes({
        addon: hybridCollectors.addon,
      });
    },
    'Heuristic shape fetch failed'
  );
});

app.get('/heuristictype', async (_req, res) => {
  return respondJson(
    res,
    async () => {
      const heuristicType = await readHeuristicTypeFromDatabase();
      return { heuristicType: heuristicType || null };
    },
    'Heuristic type fetch failed'
  );
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
      req.query.decodeSerialized,
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
  return respondJson(res, readWorkersSnapshot, 'Workers snapshot failed', false);
});

app.get('/shard-placement', async (_req, res) => {
  return respondJson(
    res,
    readShardPlacementFromDatabase,
    'Shard placement fetch failed',
    false
  );
});

const PORT = Number(process.env.CARTOGRAPH_NATIVE_PORT || DEFAULT_PORT);
app.listen(PORT, '0.0.0.0', () => {
  console.log(`Native server running on port ${PORT}`);
});
