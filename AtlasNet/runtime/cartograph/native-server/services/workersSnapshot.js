const { spawn } = require('node:child_process');
const fs = require('node:fs');
const http = require('node:http');
const https = require('node:https');
const { URL } = require('node:url');

const DOCKER_COMMAND_TIMEOUT_MS = 3500;
const K8S_COMMAND_TIMEOUT_MS = 3500;
const MAX_CONTEXTS = 64;
const MAX_CONTAINERS_PER_CONTEXT = 300;
const DEFAULT_DOCKER_SOCKET_PATH = '/var/run/docker.sock';
const MAX_LOG_CHARS_PER_NODE = 24 * 1024;
const MAX_LOG_SOURCES_PER_NODE = 10;
const NODE_LOG_CACHE_TTL_MS = 4000;

const k8sNodeLogCache = new Map();
const dockerNodeLogCache = new Map();

function asObject(value) {
  return value && typeof value === 'object' ? value : null;
}

function asOptionalString(value) {
  if (value == null) {
    return null;
  }
  const text = String(value).trim();
  return text.length > 0 ? text : null;
}

function asString(value, fallback = '') {
  const normalized = asOptionalString(value);
  if (normalized != null) {
    return normalized;
  }
  return asOptionalString(fallback) ?? '';
}

function asBoolean(value, defaultValue = false) {
  if (typeof value === 'boolean') {
    return value;
  }
  const normalized = String(value ?? '').trim().toLowerCase();
  if (
    normalized === '1' ||
    normalized === 'true' ||
    normalized === 'yes' ||
    normalized === 'on' ||
    normalized === '*'
  ) {
    return true;
  }
  if (
    normalized === '0' ||
    normalized === 'false' ||
    normalized === 'no' ||
    normalized === 'off'
  ) {
    return false;
  }
  return defaultValue;
}

function asFiniteNumber(value, fallback = 0) {
  const n = Number(value);
  return Number.isFinite(n) ? n : fallback;
}

function asOptionalFiniteNumber(value) {
  if (value == null) {
    return null;
  }
  const n = Number(value);
  return Number.isFinite(n) ? n : null;
}

function safePercent(numerator, denominator) {
  if (!Number.isFinite(numerator) || !Number.isFinite(denominator) || denominator <= 0) {
    return null;
  }
  const pct = (numerator / denominator) * 100;
  return Number.isFinite(pct) ? Math.max(0, Math.min(100, pct)) : null;
}

function truncateText(value, maxChars = MAX_LOG_CHARS_PER_NODE) {
  const text = String(value ?? '');
  if (text.length <= maxChars) {
    return text;
  }
  return `${text.slice(0, maxChars)}\n...[truncated ${text.length - maxChars} chars]`;
}

function parseJsonLines(stdout) {
  const lines = String(stdout ?? '')
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter((line) => line.length > 0);
  const out = [];
  for (const line of lines) {
    try {
      out.push(JSON.parse(line));
    } catch {}
  }
  return out;
}

function parseSingleJson(stdout) {
  const rows = parseJsonLines(stdout);
  return rows[0] ?? null;
}

function deriveHostFromEndpoint(endpoint) {
  const value = asOptionalString(endpoint);
  if (!value) {
    return null;
  }
  if (value.startsWith('unix://') || value.startsWith('npipe://')) {
    return value;
  }

  try {
    const url = new URL(value);
    if (!url.hostname) {
      return value;
    }
    return url.port ? `${url.hostname}:${url.port}` : url.hostname;
  } catch {
    return value;
  }
}

function summarizeCommandError(result) {
  if (!result) {
    return 'unknown command failure';
  }

  if (result.spawnError) {
    return result.spawnError;
  }

  if (result.timedOut) {
    return `timed out after ${result.timeoutMs}ms`;
  }

  const stderr = String(result.stderr ?? '').trim();
  if (stderr.length > 0) {
    const lines = stderr
      .split(/\r?\n/)
      .map((line) => line.trim())
      .filter((line) => line.length > 0);
    if (lines.length > 0) {
      return lines[lines.length - 1];
    }
  }

  return `exit code ${result.exitCode}`;
}

function parseJsonBody(body) {
  if (!body || typeof body !== 'string') {
    return null;
  }
  try {
    return JSON.parse(body);
  } catch {
    return null;
  }
}

function readTextFileIfExists(path) {
  try {
    if (!path || !fs.existsSync(path)) {
      return null;
    }
    return String(fs.readFileSync(path, 'utf8')).trim();
  } catch {
    return null;
  }
}

function resolveKubernetesApiConfig() {
  const host = asOptionalString(process.env.KUBERNETES_SERVICE_HOST);
  if (!host) {
    return null;
  }
  const port = asOptionalString(process.env.KUBERNETES_SERVICE_PORT_HTTPS) ?? '443';
  const namespace =
    asOptionalString(process.env.POD_NAMESPACE) ??
    readTextFileIfExists('/var/run/secrets/kubernetes.io/serviceaccount/namespace') ??
    'default';
  const token = readTextFileIfExists('/var/run/secrets/kubernetes.io/serviceaccount/token');
  if (!token) {
    return null;
  }
  const ca = readTextFileIfExists('/var/run/secrets/kubernetes.io/serviceaccount/ca.crt');

  return {
    host,
    port,
    namespace,
    token,
    ca,
    endpoint: `https://${host}:${port}`,
  };
}

function kubernetesApiRequest(api, path, timeoutMs = K8S_COMMAND_TIMEOUT_MS) {
  return new Promise((resolve) => {
    let body = '';
    let settled = false;

    const done = (result) => {
      if (settled) {
        return;
      }
      settled = true;
      resolve(result);
    };

    const req = https.request(
      {
        method: 'GET',
        hostname: api.host,
        port: Number(api.port),
        path,
        headers: {
          Authorization: `Bearer ${api.token}`,
          Accept: 'application/json',
        },
        rejectUnauthorized: true,
        ca: api.ca || undefined,
      },
      (res) => {
        res.setEncoding('utf8');
        res.on('data', (chunk) => {
          body += String(chunk);
        });
        res.on('end', () => {
          const statusCode = Number(res.statusCode ?? 0);
          done({
            ok: statusCode >= 200 && statusCode < 300,
            statusCode,
            body,
            error: null,
          });
        });
      }
    );

    req.on('error', (err) => {
      done({
        ok: false,
        statusCode: 0,
        body,
        error: err instanceof Error ? err.message : String(err),
      });
    });

    req.setTimeout(timeoutMs, () => {
      req.destroy(new Error(`timed out after ${timeoutMs}ms`));
    });

    req.end();
  });
}

function parseK8sCpuQuantity(raw) {
  const value = asString(raw);
  if (!value) {
    return 0;
  }
  if (value.endsWith('m')) {
    return asFiniteNumber(value.slice(0, -1), 0) / 1000;
  }
  return asFiniteNumber(value, 0);
}

function parseK8sMemoryQuantity(raw) {
  const value = asString(raw);
  if (!value) {
    return 0;
  }

  const match = value.match(/^([0-9]+(?:\.[0-9]+)?)([KMGTE]i?)?$/i);
  if (!match) {
    return asFiniteNumber(value, 0);
  }

  const num = asFiniteNumber(match[1], 0);
  const unit = String(match[2] ?? '').toUpperCase();

  const multipliers = {
    '': 1,
    K: 1_000,
    M: 1_000_000,
    G: 1_000_000_000,
    T: 1_000_000_000_000,
    E: 1_000_000_000_000_000,
    KI: 1024,
    MI: 1024 ** 2,
    GI: 1024 ** 3,
    TI: 1024 ** 4,
    EI: 1024 ** 5,
  };

  return Math.round(num * (multipliers[unit] ?? 1));
}

function nodeAddressForDisplay(node) {
  const addresses = Array.isArray(node?.status?.addresses) ? node.status.addresses : [];
  const preferredTypes = new Set(['InternalIP', 'ExternalIP', 'Hostname']);
  for (const addr of addresses) {
    const type = asString(addr?.type);
    const address = asString(addr?.address);
    if (address && preferredTypes.has(type)) {
      return address;
    }
  }
  return '';
}

function parseK8sNodeRows(rawNodes) {
  const items = Array.isArray(rawNodes?.items) ? rawNodes.items : [];
  return items.map((node) => {
    const metadata = asObject(node?.metadata) ?? {};
    const spec = asObject(node?.spec) ?? {};
    const status = asObject(node?.status) ?? {};
    const nodeInfo = asObject(status.nodeInfo) ?? {};
    const conditions = Array.isArray(status.conditions) ? status.conditions : [];
    const ready = conditions.find((condition) => asString(condition?.type) === 'Ready');
    const allocatable = asObject(status.allocatable) ?? {};
    const hostname = asString(metadata.name) || asString(nodeInfo.hostname);
    return {
      id: asString(metadata.uid) || hostname,
      hostname: hostname || asString(metadata.uid),
      status: asString(ready?.status, 'Unknown'),
      availability: spec.unschedulable ? 'cordoned' : 'active',
      managerStatus: 'kubernetes-node',
      engineVersion: asString(nodeInfo.kubeletVersion),
      tlsStatus: null,
      address: nodeAddressForDisplay(node),
      cpuCapacityCores: parseK8sCpuQuantity(allocatable.cpu),
      memoryCapacityBytes: parseK8sMemoryQuantity(allocatable.memory),
      cpuUsageCores: null,
      memoryUsageBytes: null,
      cpuUsagePct: null,
      memoryUsagePct: null,
      containers: [],
      aggregateLogs: '',
    };
  });
}

function parseK8sNodeRowsFromPods(rawPods) {
  const items = Array.isArray(rawPods?.items) ? rawPods.items : [];
  const nodeNames = new Set();
  for (const pod of items) {
    const nodeName = asString(pod?.spec?.nodeName);
    if (nodeName) {
      nodeNames.add(nodeName);
    }
  }

  return Array.from(nodeNames.values()).map((nodeName) => ({
    id: nodeName,
    hostname: nodeName,
    status: 'Unknown',
    availability: 'active',
    managerStatus: 'kubernetes-node',
    engineVersion: '',
    tlsStatus: null,
    address: null,
    cpuCapacityCores: null,
    memoryCapacityBytes: null,
    cpuUsageCores: null,
    memoryUsageBytes: null,
    cpuUsagePct: null,
    memoryUsagePct: null,
    containers: [],
    aggregateLogs: '',
  }));
}

function parseK8sNodeMetricsMap(rawNodeMetrics) {
  const items = Array.isArray(rawNodeMetrics?.items) ? rawNodeMetrics.items : [];
  const usageByNode = new Map();
  for (const item of items) {
    const metadata = asObject(item?.metadata) ?? {};
    const usage = asObject(item?.usage) ?? {};
    const nodeName = asString(metadata.name);
    if (!nodeName) {
      continue;
    }
    usageByNode.set(nodeName, {
      cpuUsageCores: parseK8sCpuQuantity(usage.cpu),
      memoryUsageBytes: parseK8sMemoryQuantity(usage.memory),
    });
  }
  return usageByNode;
}

function collectK8sPodContainersByNode(rawPods) {
  const items = Array.isArray(rawPods?.items) ? rawPods.items : [];
  const byNode = new Map();
  for (const pod of items) {
    const podName = asString(pod?.metadata?.name);
    const nodeName = asString(pod?.spec?.nodeName);
    if (!podName || !nodeName) {
      continue;
    }

    const containers = Array.isArray(pod?.spec?.containers) ? pod.spec.containers : [];
    if (!byNode.has(nodeName)) {
      byNode.set(nodeName, []);
    }
    const current = byNode.get(nodeName);
    for (const container of containers) {
      const containerName = asString(container?.name);
      if (!containerName) {
        continue;
      }
      current.push({
        podName,
        containerName,
      });
    }
  }
  return byNode;
}

async function collectK8sNodeAggregateLogs(api, podContainersByNode) {
  const nowMs = Date.now();
  const out = new Map();

  for (const [nodeName, podContainers] of podContainersByNode.entries()) {
    const cacheKey = `${api.namespace}:${nodeName}`;
    const cached = k8sNodeLogCache.get(cacheKey);
    if (cached && nowMs - cached.atMs < NODE_LOG_CACHE_TTL_MS) {
      out.set(nodeName, cached.text);
      continue;
    }

    const selectedSources = podContainers.slice(0, MAX_LOG_SOURCES_PER_NODE);
    const chunks = await Promise.all(
      selectedSources.map(async (source) => {
        const podName = asString(source?.podName);
        const containerName = asString(source?.containerName);
        if (!podName || !containerName) {
          return '';
        }

        const path =
          `/api/v1/namespaces/${encodeURIComponent(api.namespace)}/pods/` +
          `${encodeURIComponent(podName)}/log?container=${encodeURIComponent(containerName)}` +
          '&tailLines=40&timestamps=true';

        const result = await kubernetesApiRequest(api, path, 1200);
        if (!result.ok) {
          return '';
        }

        const body = asString(result.body);
        if (!body) {
          return '';
        }

        return `[${podName}/${containerName}]\n${body}`;
      })
    );

    const text = truncateText(chunks.filter((chunk) => chunk.length > 0).join('\n\n'));
    k8sNodeLogCache.set(cacheKey, {
      atMs: nowMs,
      text,
    });
    out.set(nodeName, text);
  }

  return out;
}

function formatK8sContainerPorts(container) {
  const ports = Array.isArray(container?.ports) ? container.ports : [];
  return ports
    .map((port) => {
      const number = asFiniteNumber(port?.containerPort, 0);
      const protocol = asString(port?.protocol || 'TCP').toLowerCase();
      if (number <= 0) {
        return '';
      }
      return `${number}/${protocol}`;
    })
    .filter((value) => value.length > 0)
    .join(', ');
}

function formatIsoTimestamp(value) {
  const text = asString(value);
  if (!text) {
    return '';
  }
  const date = new Date(text);
  return Number.isNaN(date.getTime()) ? '' : date.toISOString();
}

function elapsedSinceIso(value) {
  const text = asString(value);
  if (!text) {
    return '';
  }
  const ts = new Date(text).getTime();
  if (!Number.isFinite(ts)) {
    return '';
  }
  const elapsedSeconds = Math.max(0, Math.floor((Date.now() - ts) / 1000));
  if (elapsedSeconds < 60) {
    return `${elapsedSeconds}s`;
  }
  if (elapsedSeconds < 3600) {
    return `${Math.floor(elapsedSeconds / 60)}m`;
  }
  if (elapsedSeconds < 86400) {
    return `${Math.floor(elapsedSeconds / 3600)}h`;
  }
  return `${Math.floor(elapsedSeconds / 86400)}d`;
}

function parseK8sContainerRows(rawPods) {
  const items = Array.isArray(rawPods?.items) ? rawPods.items : [];
  const out = [];

  for (const pod of items) {
    const podName = asString(pod?.metadata?.name);
    const nodeId = asString(pod?.spec?.nodeName);
    const podPhase = asString(pod?.status?.phase);
    const startTime = asString(pod?.status?.startTime || pod?.metadata?.creationTimestamp);
    const containers = Array.isArray(pod?.spec?.containers) ? pod.spec.containers : [];
    const statuses = Array.isArray(pod?.status?.containerStatuses)
      ? pod.status.containerStatuses
      : [];

    const statusByName = new Map();
    for (const st of statuses) {
      const name = asString(st?.name);
      if (name) {
        statusByName.set(name, st);
      }
    }

    for (const container of containers) {
      const name = asString(container?.name);
      const status = statusByName.get(name) ?? {};
      const stateObj = asObject(status?.state) ?? {};
      const state =
        (Object.keys(stateObj)[0] && String(Object.keys(stateObj)[0])) || podPhase || 'unknown';
      const image = asString(status?.image || container?.image);
      const ready = Boolean(status?.ready);

      out.push({
        id: asString(status?.containerID || `${podName}/${name}`),
        name: `${podName}/${name}`,
        image,
        state,
        status: `${podPhase}${ready ? ' (ready)' : ''}`.trim(),
        command: Array.isArray(container?.command) ? container.command.join(' ') : '',
        runningFor: elapsedSinceIso(startTime),
        createdAt: formatIsoTimestamp(startTime),
        ports: formatK8sContainerPorts(container),
        nodeId: nodeId || undefined,
      });
    }
  }

  return out;
}

function buildK8sDaemonInfo(api, rawVersion, rawNodes, rawPods) {
  const version = asObject(rawVersion) ?? {};
  const nodeItems = Array.isArray(rawNodes?.items) ? rawNodes.items : [];
  const podItems = Array.isArray(rawPods?.items) ? rawPods.items : [];
  const firstNode = nodeItems[0] ?? {};
  const nodeInfo = asObject(firstNode?.status?.nodeInfo) ?? {};

  let cpuTotal = 0;
  let memoryTotalBytes = 0;
  for (const node of nodeItems) {
    cpuTotal += parseK8sCpuQuantity(node?.status?.allocatable?.cpu);
    memoryTotalBytes += parseK8sMemoryQuantity(node?.status?.allocatable?.memory);
  }

  const runningPodCount = podItems.filter(
    (pod) => asString(pod?.status?.phase).toLowerCase() === 'running'
  ).length;

  return {
    id: asString(firstNode?.metadata?.uid, 'kubernetes'),
    name: asString(nodeInfo.hostname || firstNode?.metadata?.name || api.namespace),
    serverVersion: asString(version.gitVersion),
    operatingSystem: asString(nodeInfo.operatingSystem, 'kubernetes'),
    osVersion: asString(nodeInfo.osImage),
    kernelVersion: asString(nodeInfo.kernelVersion),
    architecture: asString(nodeInfo.architecture),
    cpuCount: Math.round(cpuTotal),
    memoryTotalBytes,
    containersTotal: podItems.length,
    containersRunning: runningPodCount,
    containersPaused: 0,
    containersStopped: Math.max(0, podItems.length - runningPodCount),
    swarmNodeId: asString(firstNode?.metadata?.uid),
    swarmNodeAddress: nodeAddressForDisplay(firstNode),
    swarmState: 'kubernetes',
    swarmControlAvailable: true,
  };
}

function extractK8sApiError(result, fallback) {
  if (!result) {
    return fallback;
  }
  if (result.error) {
    return result.error;
  }
  const parsed = parseJsonBody(result.body);
  const fromMessage = asOptionalString(parsed?.message);
  if (fromMessage) {
    return fromMessage;
  }
  if (result.statusCode > 0) {
    return `Kubernetes API request failed (${result.statusCode})`;
  }
  return fallback;
}

async function readWorkersSnapshotViaKubernetes() {
  const api = resolveKubernetesApiConfig();
  if (!api) {
    return null;
  }

  const [versionResult, podsResult, nodesResult, nodeMetricsResult] = await Promise.all([
    kubernetesApiRequest(api, '/version'),
    kubernetesApiRequest(api, `/api/v1/namespaces/${api.namespace}/pods`),
    kubernetesApiRequest(api, '/api/v1/nodes'),
    kubernetesApiRequest(api, '/apis/metrics.k8s.io/v1beta1/nodes'),
  ]);

  const endpoint = api.endpoint;
  const contextBase = {
    name: `kubernetes:${api.namespace}`,
    description: `Kubernetes namespace ${api.namespace}`,
    dockerEndpoint: endpoint,
    host: `${api.host}:${api.port}`,
    current: true,
    orchestrator: 'kubernetes',
    source: 'in-cluster service account',
    status: 'ok',
    error: null,
    daemon: null,
    nodes: [],
    containers: [],
  };

  if (!versionResult.ok || !podsResult.ok) {
    const error =
      (!versionResult.ok &&
        extractK8sApiError(versionResult, 'failed to query Kubernetes version')) ||
      extractK8sApiError(podsResult, 'failed to query Kubernetes pods');
    return {
      collectedAtMs: Date.now(),
      dockerCliAvailable: true,
      error: null,
      contexts: [
        {
          ...contextBase,
          status: 'error',
          error,
        },
      ],
    };
  }

  const parsedVersion = parseJsonBody(versionResult.body);
  const parsedPods = parseJsonBody(podsResult.body);
  const parsedNodes = nodesResult.ok ? parseJsonBody(nodesResult.body) : null;
  const allContainers = parseK8sContainerRows(parsedPods);
  const containers = allContainers.slice(0, MAX_CONTAINERS_PER_CONTEXT);

  const daemon = buildK8sDaemonInfo(api, parsedVersion, parsedNodes, parsedPods);
  const nodeRows = nodesResult.ok
    ? parseK8sNodeRows(parsedNodes)
    : parseK8sNodeRowsFromPods(parsedPods);
  const metricsByNode = nodeMetricsResult.ok
    ? parseK8sNodeMetricsMap(parseJsonBody(nodeMetricsResult.body))
    : new Map();

  const containersByNode = new Map();
  for (const container of allContainers) {
    const nodeId = asString(container?.nodeId);
    if (!nodeId) {
      continue;
    }
    if (!containersByNode.has(nodeId)) {
      containersByNode.set(nodeId, []);
    }
    containersByNode.get(nodeId).push(container);
  }

  const podContainersByNode = collectK8sPodContainersByNode(parsedPods);
  const logsByNode = await collectK8sNodeAggregateLogs(api, podContainersByNode);

  const nodes = nodeRows.map((node) => {
    const nodeId = asString(node.hostname || node.id);
    const nodeContainers = (containersByNode.get(nodeId) ?? []).slice(
      0,
      MAX_CONTAINERS_PER_CONTEXT
    );
    const metric = metricsByNode.get(nodeId);

    const cpuCapacityCores = asOptionalFiniteNumber(node.cpuCapacityCores);
    const memoryCapacityBytes = asOptionalFiniteNumber(node.memoryCapacityBytes);
    const cpuUsageCores = metric ? metric.cpuUsageCores : null;
    const memoryUsageBytes = metric ? metric.memoryUsageBytes : null;

    return {
      ...node,
      cpuUsageCores,
      memoryUsageBytes,
      cpuUsagePct: safePercent(cpuUsageCores, cpuCapacityCores),
      memoryUsagePct: safePercent(memoryUsageBytes, memoryCapacityBytes),
      containers: nodeContainers,
      aggregateLogs: logsByNode.get(nodeId) ?? '',
    };
  });

  if (nodes.length === 0) {
    for (const [nodeId, nodeContainers] of containersByNode.entries()) {
      nodes.push({
        id: nodeId,
        hostname: nodeId,
        status: 'Unknown',
        availability: 'active',
        managerStatus: 'kubernetes-node',
        engineVersion: '',
        tlsStatus: null,
        address: null,
        cpuCapacityCores: null,
        memoryCapacityBytes: null,
        cpuUsageCores: null,
        memoryUsageBytes: null,
        cpuUsagePct: null,
        memoryUsagePct: null,
        containers: nodeContainers.slice(0, MAX_CONTAINERS_PER_CONTEXT),
        aggregateLogs: logsByNode.get(nodeId) ?? '',
      });
    }
  }

  return {
    collectedAtMs: Date.now(),
    dockerCliAvailable: true,
    error: null,
    contexts: [
      {
        ...contextBase,
        daemon,
        nodes,
        containers,
      },
    ],
  };
}

function extractDockerSocketPath() {
  const candidates = [];
  const explicit = asOptionalString(process.env.CARTOGRAPH_DOCKER_SOCKET_PATH);
  if (explicit) {
    return explicit;
  }

  const raw = asOptionalString(process.env.DOCKER_HOST);
  if (raw && raw.startsWith('unix://')) {
    const fromDockerHost = raw.slice('unix://'.length).trim();
    if (fromDockerHost.length > 0) {
      candidates.push(fromDockerHost);
    }
  }

  candidates.push(DEFAULT_DOCKER_SOCKET_PATH);
  candidates.push('/run/docker.sock');

  const xdgRuntimeDir = asOptionalString(process.env.XDG_RUNTIME_DIR);
  if (xdgRuntimeDir) {
    candidates.push(`${xdgRuntimeDir.replace(/\/+$/, '')}/docker.sock`);
  }

  const uid = typeof process.getuid === 'function' ? process.getuid() : null;
  if (uid != null) {
    candidates.push(`/run/user/${uid}/docker.sock`);
  }

  const unique = Array.from(new Set(candidates.filter((candidate) => candidate.length > 0)));
  for (const candidate of unique) {
    try {
      if (fs.existsSync(candidate)) {
        return candidate;
      }
    } catch {}
  }

  return unique[0] ?? null;
}

function getDockerEndpointForFallback(socketPath) {
  const explicit = asOptionalString(process.env.CARTOGRAPH_DOCKER_SOCKET_PATH);
  if (explicit) {
    return `unix://${explicit}`;
  }
  const raw = asOptionalString(process.env.DOCKER_HOST);
  if (raw && raw.startsWith('unix://')) {
    return raw;
  }
  return `unix://${socketPath}`;
}

function dockerApiRequest(socketPath, path, timeoutMs = DOCKER_COMMAND_TIMEOUT_MS) {
  return new Promise((resolve) => {
    let body = '';
    let settled = false;

    const done = (result) => {
      if (settled) {
        return;
      }
      settled = true;
      resolve(result);
    };

    const req = http.request(
      {
        method: 'GET',
        socketPath,
        path,
      },
      (res) => {
        res.setEncoding('utf8');
        res.on('data', (chunk) => {
          body += String(chunk);
        });
        res.on('end', () => {
          const statusCode = Number(res.statusCode ?? 0);
          done({
            ok: statusCode >= 200 && statusCode < 300,
            statusCode,
            body,
            error: null,
          });
        });
      }
    );

    req.on('error', (err) => {
      done({
        ok: false,
        statusCode: 0,
        body,
        error: err instanceof Error ? err.message : String(err),
      });
    });

    req.setTimeout(timeoutMs, () => {
      req.destroy(new Error(`timed out after ${timeoutMs}ms`));
    });

    req.end();
  });
}

function parseContextRows(stdout) {
  const rows = parseJsonLines(stdout);
  return rows.map((raw) => {
    const row = asObject(raw) ?? {};
    return {
      name: asString(row.Name),
      description: asString(row.Description),
      dockerEndpoint: asString(row.DockerEndpoint),
      host: deriveHostFromEndpoint(row.DockerEndpoint),
      current: asBoolean(row.Current, false),
      orchestrator: asOptionalString(row.Orchestrator),
      source: null,
      status: 'ok',
      error: asOptionalString(row.Error),
      daemon: null,
      nodes: [],
      containers: [],
    };
  });
}

function parseContextInspect(stdout) {
  const parsed = parseSingleJson(stdout);
  const root = asObject(parsed);
  if (!root) {
    return null;
  }

  const storage = asObject(root.Storage) ?? {};
  const endpoints = asObject(root.Endpoints) ?? {};
  const dockerEndpoint = asObject(endpoints.docker) ?? {};

  return {
    source: asOptionalString(storage.MetadataPath) ?? asOptionalString(storage.TLSPath),
    dockerEndpoint: asOptionalString(dockerEndpoint.Host),
  };
}

function parseDaemonInfo(stdout) {
  const parsed = parseSingleJson(stdout);
  const root = asObject(parsed);
  if (!root) {
    return null;
  }
  const swarm = asObject(root.Swarm) ?? {};

  return {
    id: asString(root.ID),
    name: asString(root.Name),
    serverVersion: asString(root.ServerVersion),
    operatingSystem: asString(root.OperatingSystem),
    osVersion: asString(root.OSVersion),
    kernelVersion: asString(root.KernelVersion),
    architecture: asString(root.Architecture),
    cpuCount: asFiniteNumber(root.NCPU),
    memoryTotalBytes: asFiniteNumber(root.MemTotal),
    containersTotal: asFiniteNumber(root.Containers),
    containersRunning: asFiniteNumber(root.ContainersRunning),
    containersPaused: asFiniteNumber(root.ContainersPaused),
    containersStopped: asFiniteNumber(root.ContainersStopped),
    swarmNodeId: asString(swarm.NodeID),
    swarmNodeAddress: asString(swarm.NodeAddr),
    swarmState: asString(swarm.LocalNodeState),
    swarmControlAvailable: Boolean(swarm.ControlAvailable),
  };
}

function parseNodeRows(stdout) {
  const rows = parseJsonLines(stdout);
  return rows.map((raw) => {
    const row = asObject(raw) ?? {};
    return {
      id: asString(row.ID),
      hostname: asString(row.Hostname),
      status: asString(row.Status),
      availability: asString(row.Availability),
      managerStatus: asOptionalString(row.ManagerStatus),
      engineVersion: asString(row.EngineVersion),
      tlsStatus: asOptionalString(row.TLSStatus),
    };
  });
}

function parseContainerRows(stdout) {
  const rows = parseJsonLines(stdout);
  return rows.map((raw) => {
    const row = asObject(raw) ?? {};
    return {
      id: asString(row.ID),
      name: asString(row.Names),
      image: asString(row.Image),
      state: asString(row.State),
      status: asString(row.Status),
      command: asString(row.Command),
      runningFor: asString(row.RunningFor),
      createdAt: asString(row.CreatedAt),
      ports: asString(row.Ports),
    };
  });
}

function parseSwarmNodeRowsFromApi(raw) {
  if (!Array.isArray(raw)) {
    return [];
  }
  return raw.map((rowRaw) => {
    const row = asObject(rowRaw) ?? {};
    const description = asObject(row.Description) ?? {};
    const status = asObject(row.Status) ?? {};
    const spec = asObject(row.Spec) ?? {};
    const managerStatus = asObject(row.ManagerStatus) ?? null;
    const engine = asObject(description.Engine) ?? {};

    let managerState = null;
    if (managerStatus) {
      const leader = Boolean(managerStatus.Leader);
      const reachability = asString(managerStatus.Reachability);
      const addr = asString(managerStatus.Addr);
      managerState = `${leader ? 'leader' : reachability || 'manager'}${addr ? ` (${addr})` : ''}`;
    }

    return {
      id: asString(row.ID),
      hostname: asString(description.Hostname),
      status: asString(status.State),
      availability: asString(spec.Availability),
      managerStatus: managerState,
      engineVersion: asString(engine.EngineVersion),
      tlsStatus: asOptionalString(status.TLSStatus),
    };
  });
}

function formatContainerPortsFromApi(portsRaw) {
  if (!Array.isArray(portsRaw) || portsRaw.length === 0) {
    return '';
  }
  return portsRaw
    .map((portRaw) => {
      const port = asObject(portRaw) ?? {};
      const privatePort = asFiniteNumber(port.PrivatePort, 0);
      const publicPort = asFiniteNumber(port.PublicPort, 0);
      const type = asString(port.Type, 'tcp').toLowerCase();
      const ip = asString(port.IP);
      if (publicPort > 0 && privatePort > 0) {
        if (ip.length > 0) {
          return `${ip}:${publicPort}->${privatePort}/${type}`;
        }
        return `${publicPort}->${privatePort}/${type}`;
      }
      if (privatePort > 0) {
        return `${privatePort}/${type}`;
      }
      return '';
    })
    .filter((value) => value.length > 0)
    .join(', ');
}

function formatContainerCreatedAt(rawCreatedSeconds) {
  const createdSeconds = asFiniteNumber(rawCreatedSeconds, 0);
  if (createdSeconds <= 0) {
    return '';
  }
  const createdMs = createdSeconds * 1000;
  const date = new Date(createdMs);
  return Number.isNaN(date.getTime()) ? '' : date.toISOString();
}

function formatContainerRunningFor(rawCreatedSeconds) {
  const createdSeconds = asFiniteNumber(rawCreatedSeconds, 0);
  if (createdSeconds <= 0) {
    return '';
  }
  const elapsedSeconds = Math.max(0, Math.floor(Date.now() / 1000) - createdSeconds);
  if (elapsedSeconds < 60) {
    return `${elapsedSeconds}s`;
  }
  if (elapsedSeconds < 3600) {
    return `${Math.floor(elapsedSeconds / 60)}m`;
  }
  if (elapsedSeconds < 86400) {
    return `${Math.floor(elapsedSeconds / 3600)}h`;
  }
  return `${Math.floor(elapsedSeconds / 86400)}d`;
}

function parseContainerRowsFromApi(raw) {
  if (!Array.isArray(raw)) {
    return [];
  }
  return raw.map((rowRaw) => {
    const row = asObject(rowRaw) ?? {};
    const names = Array.isArray(row.Names) ? row.Names : [];
    const firstNameRaw = names.length > 0 ? String(names[0] ?? '') : '';
    const firstName = firstNameRaw.replace(/^\/+/, '').trim();
    const createdSeconds = asFiniteNumber(row.Created, 0);

    return {
      id: asString(row.Id),
      name: firstName,
      image: asString(row.Image),
      state: asString(row.State),
      status: asString(row.Status),
      command: asString(row.Command),
      runningFor: formatContainerRunningFor(createdSeconds),
      createdAt: formatContainerCreatedAt(createdSeconds),
      ports: formatContainerPortsFromApi(row.Ports),
    };
  });
}

function parseDaemonInfoFromApi(infoPayload, versionPayload) {
  const info = asObject(infoPayload);
  if (!info) {
    return null;
  }

  const swarm = asObject(info.Swarm) ?? {};
  const version = asObject(versionPayload) ?? {};

  return {
    id: asString(info.ID),
    name: asString(info.Name),
    serverVersion: asString(info.ServerVersion) || asString(version.Version),
    operatingSystem: asString(info.OperatingSystem),
    osVersion: asString(info.OSVersion),
    kernelVersion: asString(info.KernelVersion),
    architecture: asString(info.Architecture),
    cpuCount: asFiniteNumber(info.NCPU),
    memoryTotalBytes: asFiniteNumber(info.MemTotal),
    containersTotal: asFiniteNumber(info.Containers),
    containersRunning: asFiniteNumber(info.ContainersRunning),
    containersPaused: asFiniteNumber(info.ContainersPaused),
    containersStopped: asFiniteNumber(info.ContainersStopped),
    swarmNodeId: asString(swarm.NodeID),
    swarmNodeAddress: asString(swarm.NodeAddr),
    swarmState: asString(swarm.LocalNodeState),
    swarmControlAvailable: Boolean(swarm.ControlAvailable),
  };
}

function extractDockerApiError(result, fallback) {
  if (!result) {
    return fallback;
  }
  if (result.error) {
    return result.error;
  }
  if (result.body) {
    const parsed = parseJsonBody(result.body);
    const fromMessage = asOptionalString(parsed && parsed.message);
    if (fromMessage) {
      return fromMessage;
    }
  }
  if (result.statusCode > 0) {
    return `Docker API request failed (${result.statusCode})`;
  }
  return fallback;
}

async function readWorkersSnapshotViaDockerApi(baseErrorMessage) {
  const socketPath = extractDockerSocketPath();
  const fallbackDescription = asOptionalString(baseErrorMessage) ?? 'docker CLI unavailable';

  if (!socketPath) {
    return {
      collectedAtMs: Date.now(),
      dockerCliAvailable: false,
      error: `${fallbackDescription}; unsupported DOCKER_HOST (only unix:// is supported)`,
      contexts: [],
    };
  }

  if (!fs.existsSync(socketPath)) {
    const endpoint = getDockerEndpointForFallback(socketPath);
    return {
      collectedAtMs: Date.now(),
      dockerCliAvailable: false,
      error: null,
      contexts: [
        {
          name: 'default',
          description: 'Docker Engine API fallback',
          dockerEndpoint: endpoint,
          host: deriveHostFromEndpoint(endpoint),
          current: true,
          orchestrator: null,
          source: socketPath,
          status: 'error',
          error:
            `Docker socket not found at ${socketPath}. ` +
            'Mount /var/run/docker.sock into Cartograph or set CARTOGRAPH_DOCKER_SOCKET_PATH/DOCKER_HOST.',
          daemon: null,
          nodes: [],
          containers: [],
        },
      ],
    };
  }

  const endpoint = getDockerEndpointForFallback(socketPath);
  const [infoResult, versionResult, nodesResult, containersResult] = await Promise.all([
    dockerApiRequest(socketPath, '/info'),
    dockerApiRequest(socketPath, '/version'),
    dockerApiRequest(socketPath, '/nodes'),
    dockerApiRequest(socketPath, '/containers/json?all=1'),
  ]);

  if (!infoResult.ok) {
    const infoError = extractDockerApiError(
      infoResult,
      'failed to query docker daemon via socket'
    );
    return {
      collectedAtMs: Date.now(),
      dockerCliAvailable: false,
      error: null,
      contexts: [
        {
          name: 'default',
          description: 'Docker Engine API fallback',
          dockerEndpoint: endpoint,
          host: deriveHostFromEndpoint(endpoint),
          current: true,
          orchestrator: null,
          source: socketPath,
          status: 'error',
          error: infoError,
          daemon: null,
          nodes: [],
          containers: [],
        },
      ],
    };
  }

  const daemon = parseDaemonInfoFromApi(
    parseJsonBody(infoResult.body),
    parseJsonBody(versionResult.body)
  );

  if (!daemon) {
    return {
      collectedAtMs: Date.now(),
      dockerCliAvailable: false,
      error: 'failed to parse docker daemon info',
      contexts: [],
    };
  }

  const nodes = nodesResult.ok
    ? parseSwarmNodeRowsFromApi(parseJsonBody(nodesResult.body))
    : [];
  const containersRaw = containersResult.ok
    ? parseContainerRowsFromApi(parseJsonBody(containersResult.body)).slice(
        0,
        MAX_CONTAINERS_PER_CONTEXT
      )
    : [];

  const fallbackNodeId = asString(daemon.swarmNodeId || daemon.id || daemon.name || 'default');
  const materializedNodes =
    nodes.length > 0
      ? nodes
      : [
          {
            id: fallbackNodeId,
            hostname: asString(daemon.name, fallbackNodeId),
            status: 'Ready',
            availability: 'active',
            managerStatus: daemon.swarmState || 'docker-node',
            engineVersion: daemon.serverVersion,
            tlsStatus: null,
          },
        ];

  const primaryNode = materializedNodes[0];
  const primaryNodeId = asString(primaryNode?.id || primaryNode?.hostname || fallbackNodeId);
  const containers = containersRaw.map((container) => ({
    ...container,
    nodeId: primaryNodeId,
  }));
  const enrichedNodes = materializedNodes.map((node) => {
    const nodeId = asString(node.id || node.hostname || primaryNodeId);
    const nodeContainers = nodeId === primaryNodeId ? containers : [];
    return {
      ...node,
      cpuUsageCores: null,
      cpuCapacityCores: daemon.cpuCount,
      cpuUsagePct: null,
      memoryUsageBytes: null,
      memoryCapacityBytes: daemon.memoryTotalBytes,
      memoryUsagePct: null,
      containers: nodeContainers,
      aggregateLogs: '',
    };
  });

  return {
    collectedAtMs: Date.now(),
    dockerCliAvailable: false,
    error: null,
    contexts: [
      {
        name: 'default',
        description: 'Docker Engine API fallback',
        dockerEndpoint: endpoint,
        host: deriveHostFromEndpoint(endpoint),
        current: true,
        orchestrator: null,
        source: socketPath,
        status: 'ok',
        error: null,
        daemon,
        nodes: enrichedNodes,
        containers,
      },
    ],
  };
}

function runDockerCommand(args, timeoutMs = DOCKER_COMMAND_TIMEOUT_MS) {
  return new Promise((resolve) => {
    let stdout = '';
    let stderr = '';
    let timedOut = false;
    let settled = false;
    let timeoutId = null;

    const child = spawn('docker', args, {
      stdio: ['ignore', 'pipe', 'pipe'],
    });

    const done = (result) => {
      if (settled) {
        return;
      }
      settled = true;
      if (timeoutId != null) {
        clearTimeout(timeoutId);
      }
      resolve(result);
    };

    child.stdout.on('data', (chunk) => {
      stdout += String(chunk);
    });
    child.stderr.on('data', (chunk) => {
      stderr += String(chunk);
    });

    child.on('error', (err) => {
      done({
        ok: false,
        stdout,
        stderr,
        timedOut: false,
        timeoutMs,
        exitCode: null,
        spawnError: err instanceof Error ? err.message : String(err),
      });
    });

    child.on('close', (code) => {
      done({
        ok: code === 0 && !timedOut,
        stdout,
        stderr,
        timedOut,
        timeoutMs,
        exitCode: code,
        spawnError: null,
      });
    });

    timeoutId = setTimeout(() => {
      timedOut = true;
      try {
        child.kill('SIGKILL');
      } catch {}
    }, timeoutMs);
  });
}

async function collectDockerAggregateLogs(contextName, nodeKey, containers) {
  const cacheKey = `${contextName}:${nodeKey}`;
  const nowMs = Date.now();
  const cached = dockerNodeLogCache.get(cacheKey);
  if (cached && nowMs - cached.atMs < NODE_LOG_CACHE_TTL_MS) {
    return cached.text;
  }

  const selectedContainers = (Array.isArray(containers) ? containers : []).slice(
    0,
    MAX_LOG_SOURCES_PER_NODE
  );
  const chunks = await Promise.all(
    selectedContainers.map(async (container) => {
      const containerRef = asString(container?.id || container?.name);
      const containerName = asString(container?.name, containerRef);
      if (!containerRef) {
        return '';
      }

      const result = await runDockerCommand(
        ['--context', contextName, 'logs', '--tail', '40', containerRef],
        1200
      );
      if (!result.ok) {
        return '';
      }

      const logBody = asString(result.stdout);
      if (!logBody) {
        return '';
      }

      return `[${containerName}]\n${logBody}`;
    })
  );

  const text = truncateText(chunks.filter((chunk) => chunk.length > 0).join('\n\n'));
  dockerNodeLogCache.set(cacheKey, {
    atMs: nowMs,
    text,
  });
  return text;
}

async function enrichContext(baseContext) {
  const contextName = baseContext.name;
  const [inspectResult, infoResult] = await Promise.all([
    runDockerCommand(
      ['context', 'inspect', contextName, '--format', '{{json .}}'],
      1200
    ),
    runDockerCommand(['--context', contextName, 'info', '--format', '{{json .}}']),
  ]);

  const inspect = inspectResult.ok ? parseContextInspect(inspectResult.stdout) : null;
  const endpoint = inspect?.dockerEndpoint ?? baseContext.dockerEndpoint;

  const merged = {
    ...baseContext,
    dockerEndpoint: endpoint,
    host: deriveHostFromEndpoint(endpoint),
    source: inspect?.source ?? baseContext.source,
  };

  if (!infoResult.ok) {
    return {
      ...merged,
      status: 'error',
      error: summarizeCommandError(infoResult),
    };
  }

  const daemon = parseDaemonInfo(infoResult.stdout);
  if (!daemon) {
    return {
      ...merged,
      status: 'error',
      error: 'unable to parse docker info output',
    };
  }

  const [psResult, nodesResult] = await Promise.all([
    runDockerCommand([
      '--context',
      contextName,
      'ps',
      '-a',
      '--format',
      '{{json .}}',
      '--no-trunc',
    ]),
    daemon.swarmState.toLowerCase() === 'active' && daemon.swarmControlAvailable
      ? runDockerCommand([
          '--context',
          contextName,
          'node',
          'ls',
          '--format',
          '{{json .}}',
        ])
      : Promise.resolve(null),
  ]);

  const containersRaw = psResult.ok
    ? parseContainerRows(psResult.stdout).slice(0, MAX_CONTAINERS_PER_CONTEXT)
    : [];
  const nodesRaw = nodesResult && nodesResult.ok ? parseNodeRows(nodesResult.stdout) : [];

  const fallbackNodeId = asString(daemon.swarmNodeId || daemon.id || daemon.name || contextName);
  const materializedNodes =
    nodesRaw.length > 0
      ? nodesRaw
      : [
          {
            id: fallbackNodeId,
            hostname: asString(daemon.name, fallbackNodeId),
            status: 'Ready',
            availability: 'active',
            managerStatus: daemon.swarmState || 'docker-node',
            engineVersion: daemon.serverVersion,
            tlsStatus: null,
          },
        ];

  const primaryNode = materializedNodes[0];
  const primaryNodeId = asString(primaryNode?.id || primaryNode?.hostname || fallbackNodeId);
  const containers = containersRaw.map((container) => ({
    ...container,
    nodeId: primaryNodeId,
  }));
  const aggregateLogs = await collectDockerAggregateLogs(contextName, primaryNodeId, containers);
  const nodes = materializedNodes.map((node) => {
    const nodeId = asString(node.id || node.hostname || primaryNodeId);
    const nodeContainers = nodeId === primaryNodeId ? containers : [];
    return {
      ...node,
      cpuUsageCores: null,
      cpuCapacityCores: daemon.cpuCount,
      cpuUsagePct: null,
      memoryUsageBytes: null,
      memoryCapacityBytes: daemon.memoryTotalBytes,
      memoryUsagePct: null,
      containers: nodeContainers,
      aggregateLogs: nodeId === primaryNodeId ? aggregateLogs : '',
    };
  });

  return {
    ...merged,
    status: 'ok',
    error: merged.error,
    daemon,
    nodes,
    containers,
  };
}

async function readWorkersSnapshot() {
  const k8sSnapshot = await readWorkersSnapshotViaKubernetes();
  if (k8sSnapshot) {
    return k8sSnapshot;
  }

  const contextsResult = await runDockerCommand([
    'context',
    'ls',
    '--format',
    '{{json .}}',
  ]);

  if (
    contextsResult.spawnError &&
    String(contextsResult.spawnError).toLowerCase().includes('enoent')
  ) {
    return readWorkersSnapshotViaDockerApi(summarizeCommandError(contextsResult));
  }

  const dockerCliAvailable =
    !contextsResult.spawnError || !contextsResult.spawnError.includes('ENOENT');

  if (!contextsResult.ok) {
    const parsedFallback = parseContextRows(contextsResult.stdout);
    return {
      collectedAtMs: Date.now(),
      dockerCliAvailable,
      error: summarizeCommandError(contextsResult),
      contexts: parsedFallback,
    };
  }

  const contexts = parseContextRows(contextsResult.stdout)
    .filter((ctx) => ctx.name.length > 0)
    .slice(0, MAX_CONTEXTS);

  const details = await Promise.all(contexts.map(enrichContext));
  details.sort((left, right) => {
    if (left.current !== right.current) {
      return left.current ? -1 : 1;
    }
    return left.name.localeCompare(right.name);
  });

  return {
    collectedAtMs: Date.now(),
    dockerCliAvailable,
    error: null,
    contexts: details,
  };
}

module.exports = {
  readWorkersSnapshot,
};
