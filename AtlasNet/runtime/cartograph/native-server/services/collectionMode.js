const COLLECTION_MODE_PURE_DATABASE = 0;
const COLLECTION_MODE_INTERLINK_HYBRID = 1;

// Toggle this number directly:
// 0 = pure_database
// 1 = interlink_hybrid
const COLLECTION_MODE = COLLECTION_MODE_INTERLINK_HYBRID;

function normalizeCollectionMode(value) {
  if (value == null) {
    return null;
  }

  if (typeof value === 'number' && Number.isFinite(value)) {
    if (value === COLLECTION_MODE_PURE_DATABASE) {
      return COLLECTION_MODE_PURE_DATABASE;
    }
    if (value === COLLECTION_MODE_INTERLINK_HYBRID) {
      return COLLECTION_MODE_INTERLINK_HYBRID;
    }
    return null;
  }

  const normalized = String(value).trim().toLowerCase();
  if (normalized.length === 0) {
    return null;
  }

  if (
    normalized === String(COLLECTION_MODE_PURE_DATABASE) ||
    normalized === 'pure_database' ||
    normalized === 'database' ||
    normalized === 'db' ||
    normalized === 'pure-db' ||
    normalized === 'puredb'
  ) {
    return COLLECTION_MODE_PURE_DATABASE;
  }

  if (
    normalized === String(COLLECTION_MODE_INTERLINK_HYBRID) ||
    normalized === 'interlink_hybrid' ||
    normalized === 'hybrid' ||
    normalized === 'interlink' ||
    normalized === 'interlink-hybrid'
  ) {
    return COLLECTION_MODE_INTERLINK_HYBRID;
  }

  return null;
}

function getDefaultCollectionMode() {
  return COLLECTION_MODE;
}

function resolveCollectionMode(requestedMode) {
  return normalizeCollectionMode(requestedMode) || getDefaultCollectionMode();
}

module.exports = {
  COLLECTION_MODE,
  COLLECTION_MODE_INTERLINK_HYBRID,
  COLLECTION_MODE_PURE_DATABASE,
  normalizeCollectionMode,
  getDefaultCollectionMode,
  resolveCollectionMode,
};
