const { spawn } = require('node:child_process');
const fs = require('node:fs');
const http = require('node:http');
const { URL } = require('node:url');

const DOCKER_COMMAND_TIMEOUT_MS = 3500;
const MAX_CONTEXTS = 64;
const MAX_CONTAINERS_PER_CONTEXT = 300;
const DEFAULT_DOCKER_SOCKET_PATH = '/var/run/docker.sock';

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

function asString(value) {
  return asOptionalString(value) ?? '';
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
  const containers = containersResult.ok
    ? parseContainerRowsFromApi(parseJsonBody(containersResult.body)).slice(
        0,
        MAX_CONTAINERS_PER_CONTEXT
      )
    : [];

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
        nodes,
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

  const containers = psResult.ok
    ? parseContainerRows(psResult.stdout).slice(0, MAX_CONTAINERS_PER_CONTEXT)
    : [];
  const nodes = nodesResult && nodesResult.ok ? parseNodeRows(nodesResult.stdout) : [];

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
