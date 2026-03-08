const { readNetworkTelemetry } = require('./networkTelemetry');
const { readEntityLedgersTelemetry } = require('./entityLedgerTelemetry');
const { readHeuristicShapes } = require('./heuristicShapes');
const {
  readAuthorityTelemetryFromDatabase,
  readHeuristicClaimedOwnersFromDatabase,
  readNetworkTelemetryFromDatabase,
  readHeuristicShapesFromDatabase,
  readTransferManifestFromDatabase,
} = require('./databaseTelemetry');

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

async function collectNetworkTelemetry({ addon, networkTelemetry }) {
  const hasAddon = Boolean(
    addon &&
      networkTelemetry &&
      addon.std_vector_std_string_ &&
      addon.std_vector_std_vector_std_string__
  );
  if (hasAddon) {
    return readNetworkTelemetry(addon, networkTelemetry);
  }
  return readNetworkTelemetryFromDatabase();
}

async function collectAuthorityTelemetry({ addon, entityLedgersView }) {
  const hasAddon = Boolean(
    addon && entityLedgersView && addon.std_vector_EntityLedgerEntry_
  );
  if (hasAddon) {
    const ownerByBoundId = await readHeuristicClaimedOwnersFromDatabase();
    return readEntityLedgersTelemetry(addon, entityLedgersView, {
      ownerByBoundId,
    });
  }
  return readAuthorityTelemetryFromDatabase();
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
