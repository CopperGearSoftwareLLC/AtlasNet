const express = require('express');
const cors = require('cors');
const addon = require('../nextjs/native/Web.node');

const { DEFAULT_PORT, getDatabaseTargets } = require('./config');
const {
  probeDatabase,
  readDatabaseRecords,
  resolveSelectedSource,
} = require('./services/databaseSnapshot');
const { readNetworkTelemetry } = require('./services/networkTelemetry');
const { readAuthorityTelemetry } = require('./services/authorityTelemetry');
const { readHeuristicShapes } = require('./services/heuristicShapes');

const app = express();
app.use(cors());

const networkTelemetry = new addon.NetworkTelemetry();
const authorityTelemetry = addon.AuthorityTelemetry
  ? new addon.AuthorityTelemetry()
  : null;

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

app.get('/networktelemetry', (_req, res) => {
  try {
    const telemetry = readNetworkTelemetry(addon, networkTelemetry);
    res.json(telemetry);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Native addon failed' });
  }
});

app.get('/authoritytelemetry', (_req, res) => {
  try {
    const rows = readAuthorityTelemetry(addon, authorityTelemetry);
    res.json(rows);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Authority telemetry fetch failed' });
  }
});

app.get('/heuristic', (_req, res) => {
  try {
    const shapes = readHeuristicShapes(addon);
    res.json(shapes);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Native addon failed' });
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

const PORT = Number(process.env.CARTOGRAPH_NATIVE_PORT || DEFAULT_PORT);
app.listen(PORT, '0.0.0.0', () => {
  console.log(`Native server running on port ${PORT}`);
});
