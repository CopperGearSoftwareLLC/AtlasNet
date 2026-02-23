const DEFAULT_PORT = 4000;
const PROBE_CONNECT_TIMEOUT_MS = 500;
const SNAPSHOT_CONNECT_TIMEOUT_MS = 700;
const MAX_PAYLOAD_CHARS = 32 * 1024;
const MAX_KEYS_PER_DB = 2000;
const SCAN_COUNT = 200;

function getDatabaseTargets() {
  return [
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
}

module.exports = {
  DEFAULT_PORT,
  PROBE_CONNECT_TIMEOUT_MS,
  SNAPSHOT_CONNECT_TIMEOUT_MS,
  MAX_PAYLOAD_CHARS,
  MAX_KEYS_PER_DB,
  SCAN_COUNT,
  getDatabaseTargets,
};
