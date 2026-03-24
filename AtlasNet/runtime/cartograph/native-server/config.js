const DEFAULT_PORT = 4000;
const PROBE_CONNECT_TIMEOUT_MS = 500;
const SNAPSHOT_CONNECT_TIMEOUT_MS = 700;
const MAX_PAYLOAD_CHARS = 32 * 1024;
const MAX_KEYS_PER_DB = 2000;
const SCAN_COUNT = 200;
const REDIS_RETRY_BASE_DELAY_MS = 250;
const REDIS_RETRY_MAX_DELAY_MS = 2000;

function getDatabaseTargets() {
  return [
    {
      id: 'internal',
      name: 'InternalDB',
      host: process.env.INTERNAL_REDIS_SERVICE_NAME || 'internaldb',
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

function createRedisConnectionOptions(target, connectTimeout) {
  return {
    host: target.host,
    port: target.port,
    lazyConnect: true,
    connectTimeout,
    maxRetriesPerRequest: 0,
    enableOfflineQueue: false,
    retryStrategy(times) {
      const baseDelay = Number(process.env.REDIS_RETRY_BASE_DELAY_MS || REDIS_RETRY_BASE_DELAY_MS);
      const maxDelay = Number(process.env.REDIS_RETRY_MAX_DELAY_MS || REDIS_RETRY_MAX_DELAY_MS);
      const nextDelay = baseDelay * Math.max(1, 2 ** (times - 1));
      return Math.min(nextDelay, maxDelay);
    },
  };
}

module.exports = {
  DEFAULT_PORT,
  PROBE_CONNECT_TIMEOUT_MS,
  SNAPSHOT_CONNECT_TIMEOUT_MS,
  MAX_PAYLOAD_CHARS,
  MAX_KEYS_PER_DB,
  SCAN_COUNT,
  REDIS_RETRY_BASE_DELAY_MS,
  REDIS_RETRY_MAX_DELAY_MS,
  createRedisConnectionOptions,
  getDatabaseTargets,
};
