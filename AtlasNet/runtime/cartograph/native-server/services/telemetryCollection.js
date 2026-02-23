const {
  COLLECTION_MODE_INTERLINK_HYBRID,
  COLLECTION_MODE_PURE_DATABASE,
  resolveCollectionMode,
} = require('./collectionMode');
const { readNetworkTelemetry } = require('./networkTelemetry');
const { readEntityLedgersTelemetry } = require('./entityLedgerTelemetry');
const { readHeuristicShapes } = require('./heuristicShapes');
const {
  readAuthorityTelemetryFromDatabase,
  readHeuristicClaimedOwnersFromDatabase,
  readNetworkTelemetryFromDatabase,
  readHeuristicShapesFromDatabase,
} = require('./pureDatabaseTelemetry');

async function collectNetworkTelemetry({
  addon,
  networkTelemetry,
  requestedMode,
}) {
  const mode = resolveCollectionMode(requestedMode);
  if (mode === COLLECTION_MODE_PURE_DATABASE) {
    return {
      modeUsed: COLLECTION_MODE_PURE_DATABASE,
      data: await readNetworkTelemetryFromDatabase(),
    };
  }

  if (mode === COLLECTION_MODE_INTERLINK_HYBRID) {
    const hasAddon = Boolean(
      addon &&
        networkTelemetry &&
        addon.std_vector_std_string_ &&
        addon.std_vector_std_vector_std_string__
    );
    if (hasAddon) {
      return {
        modeUsed: COLLECTION_MODE_INTERLINK_HYBRID,
        data: readNetworkTelemetry(addon, networkTelemetry),
      };
    }
    return {
      modeUsed: COLLECTION_MODE_PURE_DATABASE,
      data: await readNetworkTelemetryFromDatabase(),
    };
  }

  return { modeUsed: mode, data: [] };
}

async function collectAuthorityTelemetry({
  addon,
  entityLedgersView,
  requestedMode,
}) {
  const mode = resolveCollectionMode(requestedMode);
  if (mode === COLLECTION_MODE_PURE_DATABASE) {
    return {
      modeUsed: COLLECTION_MODE_PURE_DATABASE,
      data: await readAuthorityTelemetryFromDatabase(),
    };
  }

  if (mode === COLLECTION_MODE_INTERLINK_HYBRID) {
    const hasAddon = Boolean(
      addon && entityLedgersView && addon.std_vector_EntityLedgerEntry_
    );
    if (hasAddon) {
      const ownerByBoundId = await readHeuristicClaimedOwnersFromDatabase();
      return {
        modeUsed: COLLECTION_MODE_INTERLINK_HYBRID,
        data: readEntityLedgersTelemetry(addon, entityLedgersView, {
          ownerByBoundId,
        }),
      };
    }
    return {
      modeUsed: COLLECTION_MODE_PURE_DATABASE,
      data: await readAuthorityTelemetryFromDatabase(),
    };
  }

  return { modeUsed: mode, data: [] };
}

async function collectHeuristicShapes({ addon, requestedMode }) {
  const mode = resolveCollectionMode(requestedMode);
  if (mode === COLLECTION_MODE_PURE_DATABASE) {
    return {
      modeUsed: COLLECTION_MODE_PURE_DATABASE,
      data: await readHeuristicShapesFromDatabase(),
    };
  }

  if (mode === COLLECTION_MODE_INTERLINK_HYBRID) {
    const hasAddon = Boolean(
      addon && addon.HeuristicDraw && addon.std_vector_IBoundsDrawShape_
    );
    if (hasAddon) {
      return {
        modeUsed: COLLECTION_MODE_INTERLINK_HYBRID,
        data: readHeuristicShapes(addon),
      };
    }
    return {
      modeUsed: COLLECTION_MODE_PURE_DATABASE,
      data: await readHeuristicShapesFromDatabase(),
    };
  }

  return { modeUsed: mode, data: [] };
}

module.exports = {
  collectNetworkTelemetry,
  collectAuthorityTelemetry,
  collectHeuristicShapes,
};
