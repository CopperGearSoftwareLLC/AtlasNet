import type {
  WorkerContainerTelemetry,
  WorkerContextTelemetry,
  WorkerDaemonTelemetry,
  WorkersSnapshotResponse,
  WorkerSwarmNodeTelemetry,
} from '../cartographTypes';

const EMPTY_WORKERS_SNAPSHOT: WorkersSnapshotResponse = {
  collectedAtMs: null,
  dockerCliAvailable: false,
  error: null,
  contexts: [],
};

function asObject(value: unknown): Record<string, unknown> | null {
  return value && typeof value === 'object' ? (value as Record<string, unknown>) : null;
}

function asString(value: unknown, fallback = ''): string {
  if (value == null) {
    return fallback;
  }
  const text = String(value).trim();
  return text.length > 0 ? text : fallback;
}

function asOptionalString(value: unknown): string | null {
  const text = asString(value, '');
  return text.length > 0 ? text : null;
}

function asFiniteNumber(value: unknown, fallback = 0): number {
  const n = Number(value);
  return Number.isFinite(n) ? n : fallback;
}

function parseDaemon(raw: unknown): WorkerDaemonTelemetry | null {
  const daemon = asObject(raw);
  if (!daemon) {
    return null;
  }

  return {
    id: asString(daemon.id),
    name: asString(daemon.name),
    serverVersion: asString(daemon.serverVersion),
    operatingSystem: asString(daemon.operatingSystem),
    osVersion: asString(daemon.osVersion),
    kernelVersion: asString(daemon.kernelVersion),
    architecture: asString(daemon.architecture),
    cpuCount: asFiniteNumber(daemon.cpuCount),
    memoryTotalBytes: asFiniteNumber(daemon.memoryTotalBytes),
    containersTotal: asFiniteNumber(daemon.containersTotal),
    containersRunning: asFiniteNumber(daemon.containersRunning),
    containersPaused: asFiniteNumber(daemon.containersPaused),
    containersStopped: asFiniteNumber(daemon.containersStopped),
    swarmNodeId: asString(daemon.swarmNodeId),
    swarmNodeAddress: asString(daemon.swarmNodeAddress),
    swarmState: asString(daemon.swarmState),
    swarmControlAvailable: Boolean(daemon.swarmControlAvailable),
  };
}

function parseSwarmNode(raw: unknown): WorkerSwarmNodeTelemetry | null {
  const node = asObject(raw);
  if (!node) {
    return null;
  }

  return {
    id: asString(node.id),
    hostname: asString(node.hostname),
    status: asString(node.status),
    availability: asString(node.availability),
    managerStatus: asOptionalString(node.managerStatus),
    engineVersion: asString(node.engineVersion),
    tlsStatus: asOptionalString(node.tlsStatus),
  };
}

function parseContainer(raw: unknown): WorkerContainerTelemetry | null {
  const container = asObject(raw);
  if (!container) {
    return null;
  }

  return {
    id: asString(container.id),
    name: asString(container.name),
    image: asString(container.image),
    state: asString(container.state),
    status: asString(container.status),
    command: asString(container.command),
    runningFor: asString(container.runningFor),
    createdAt: asString(container.createdAt),
    ports: asString(container.ports),
  };
}

function parseContext(raw: unknown): WorkerContextTelemetry | null {
  const context = asObject(raw);
  if (!context) {
    return null;
  }

  const name = asString(context.name);
  if (!name) {
    return null;
  }

  const statusRaw = asString(context.status, 'ok');
  const status = statusRaw === 'error' ? 'error' : 'ok';

  return {
    name,
    description: asString(context.description),
    dockerEndpoint: asString(context.dockerEndpoint),
    host: asOptionalString(context.host),
    current: Boolean(context.current),
    orchestrator: asOptionalString(context.orchestrator),
    source: asOptionalString(context.source),
    status,
    error: asOptionalString(context.error),
    daemon: parseDaemon(context.daemon),
    nodes: Array.isArray(context.nodes)
      ? context.nodes
          .map(parseSwarmNode)
          .filter((node): node is WorkerSwarmNodeTelemetry => node != null)
      : [],
    containers: Array.isArray(context.containers)
      ? context.containers
          .map(parseContainer)
          .filter((container): container is WorkerContainerTelemetry => container != null)
      : [],
  };
}

export function normalizeWorkersSnapshot(raw: unknown): WorkersSnapshotResponse {
  const payload = asObject(raw);
  if (!payload) {
    return EMPTY_WORKERS_SNAPSHOT;
  }

  return {
    collectedAtMs: payload.collectedAtMs == null ? null : asFiniteNumber(payload.collectedAtMs),
    dockerCliAvailable: Boolean(payload.dockerCliAvailable),
    error: asOptionalString(payload.error),
    contexts: Array.isArray(payload.contexts)
      ? payload.contexts
          .map(parseContext)
          .filter((context): context is WorkerContextTelemetry => context != null)
      : [],
  };
}
