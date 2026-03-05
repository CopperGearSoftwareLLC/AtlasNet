'use client';

import { useEffect, useMemo, useState } from 'react';
import type {
  ShardPlacementTelemetry,
  WorkerContainerTelemetry,
  WorkerContextTelemetry,
  WorkerSwarmNodeTelemetry,
} from '../lib/cartographTypes';
import { useShardPlacement, useWorkersSnapshot } from '../lib/hooks/useTelemetryFeeds';

const DEFAULT_POLL_INTERVAL_MS = 5000;
const MIN_POLL_INTERVAL_MS = 1000;
const MAX_POLL_INTERVAL_MS = 30000;
const POLL_DISABLED_AT_MS = MAX_POLL_INTERVAL_MS;
const POLL_STEP_MS = 1000;

interface WorkerNodeView {
  key: string;
  context: WorkerContextTelemetry;
  node: WorkerSwarmNodeTelemetry;
  containers: WorkerContainerTelemetry[];
  aggregateLogs: string;
}

function formatBytes(totalBytes: number | null | undefined): string {
  if (!Number.isFinite(totalBytes) || !totalBytes || totalBytes <= 0) {
    return '0 B';
  }

  const units = ['B', 'KB', 'MB', 'GB', 'TB'];
  let value = totalBytes;
  let unitIndex = 0;
  while (value >= 1024 && unitIndex < units.length - 1) {
    value /= 1024;
    unitIndex += 1;
  }

  const digits = unitIndex === 0 ? 0 : value >= 10 ? 1 : 2;
  return `${value.toFixed(digits)} ${units[unitIndex]}`;
}

function formatUpdatedAt(updatedAtMs: number | null): string {
  if (!updatedAtMs) {
    return 'Waiting for first snapshot...';
  }
  return new Date(updatedAtMs).toLocaleTimeString();
}

function formatCpuUsageValues(
  usedCores: number | null | undefined,
  capacityCores: number | null | undefined
): string {
  if (Number.isFinite(usedCores) && Number.isFinite(capacityCores) && capacityCores && capacityCores > 0) {
    return `${usedCores!.toFixed(2)} / ${capacityCores!.toFixed(2)} cores`;
  }
  if (Number.isFinite(capacityCores) && capacityCores && capacityCores > 0) {
    return `capacity ${capacityCores!.toFixed(2)} cores`;
  }
  if (Number.isFinite(usedCores)) {
    return `${usedCores!.toFixed(2)} cores`;
  }
  return 'n/a';
}

function formatMemoryUsageValues(
  usedBytes: number | null | undefined,
  capacityBytes: number | null | undefined
): string {
  if (Number.isFinite(usedBytes) && Number.isFinite(capacityBytes) && capacityBytes && capacityBytes > 0) {
    return `${formatBytes(usedBytes)} / ${formatBytes(capacityBytes)}`;
  }
  if (Number.isFinite(capacityBytes) && capacityBytes && capacityBytes > 0) {
    return `capacity ${formatBytes(capacityBytes)}`;
  }
  if (Number.isFinite(usedBytes)) {
    return formatBytes(usedBytes);
  }
  return 'n/a';
}

function formatPct(value: number | null | undefined): string {
  if (!Number.isFinite(value)) {
    return 'n/a';
  }
  return `${value!.toFixed(1)}%`;
}

function pickMetricValue(
  containerValue: number | null | undefined,
  workerValue: number | null | undefined
): number | null | undefined {
  if (Number.isFinite(containerValue)) {
    return Number(containerValue);
  }
  return workerValue;
}

function hasContainerCpuMetrics(container: WorkerContainerTelemetry | null): boolean {
  if (!container) {
    return false;
  }
  return (
    Number.isFinite(container.cpuUsageCores) ||
    Number.isFinite(container.cpuCapacityCores) ||
    Number.isFinite(container.cpuUsagePct)
  );
}

function hasContainerMemoryMetrics(container: WorkerContainerTelemetry | null): boolean {
  if (!container) {
    return false;
  }
  return (
    Number.isFinite(container.memoryUsageBytes) ||
    Number.isFinite(container.memoryCapacityBytes) ||
    Number.isFinite(container.memoryUsagePct)
  );
}

function containerSelectionKey(container: WorkerContainerTelemetry, index: number): string {
  const id = String(container.id || '').trim();
  if (id) {
    return `id:${id}`;
  }
  const name = String(container.name || '').trim();
  if (name) {
    return `name:${name}:${index}`;
  }
  return `idx:${index}`;
}

function extractPodName(container: WorkerContainerTelemetry): string | null {
  const fullName = String(container.name || '').trim();
  if (!fullName) {
    return null;
  }
  const slashIndex = fullName.indexOf('/');
  if (slashIndex <= 0) {
    return null;
  }
  const podName = fullName.slice(0, slashIndex).trim();
  return podName || null;
}

function buildShardIdByPodName(placement: ShardPlacementTelemetry[]): Map<string, string> {
  const out = new Map<string, string>();
  for (const row of placement) {
    const podName = String(row.podName || '').trim();
    const shardId = String(row.shardId || '').trim();
    if (!podName || !shardId || out.has(podName)) {
      continue;
    }
    out.set(podName, shardId);
  }
  return out;
}

function parseAggregateLogsBySource(aggregateLogs: string): Map<string, string> {
  const out = new Map<string, string>();
  const lines = String(aggregateLogs || '').split(/\r?\n/);

  let currentSource: string | null = null;
  let buffer: string[] = [];

  function flushCurrentSource() {
    if (!currentSource) {
      return;
    }
    const text = buffer.join('\n').trimEnd();
    out.set(currentSource, text);
  }

  for (const line of lines) {
    const sourceMatch = line.match(/^\[([^\]]+)\]\s*$/);
    if (sourceMatch) {
      flushCurrentSource();
      currentSource = sourceMatch[1].trim();
      buffer = [];
      continue;
    }

    if (currentSource) {
      buffer.push(line);
    }
  }

  flushCurrentSource();
  return out;
}

function collectContainerLogCandidates(container: WorkerContainerTelemetry): string[] {
  const values = new Set<string>();

  function addCandidate(value: string | null | undefined) {
    const text = String(value || '').trim();
    if (text) {
      values.add(text);
    }
  }

  addCandidate(container.name);
  addCandidate(container.id);

  const id = String(container.id || '').trim();
  const idWithoutPrefix = id.includes('://') ? id.split('://').pop() : null;
  addCandidate(idWithoutPrefix);

  const name = String(container.name || '').trim();
  if (name.includes('/')) {
    addCandidate(name.split('/').pop());
  }

  return Array.from(values.values());
}

function resolveContainerLogs(
  container: WorkerContainerTelemetry,
  aggregateLogsBySource: Map<string, string>
): string {
  const directLogs = String(container.logs || '').trim();
  if (directLogs.length > 0) {
    return directLogs;
  }

  const candidates = collectContainerLogCandidates(container);
  for (const candidate of candidates) {
    if (aggregateLogsBySource.has(candidate)) {
      return aggregateLogsBySource.get(candidate) || '';
    }
  }

  for (const [source, text] of aggregateLogsBySource.entries()) {
    for (const candidate of candidates) {
      if (source.endsWith(`/${candidate}`)) {
        return text;
      }
    }
  }

  return '';
}

function nodeStatusPillClasses(node: WorkerSwarmNodeTelemetry): string {
  const normalized = String(node.status || '').trim().toLowerCase();
  const healthy =
    normalized === 'ready' ||
    normalized === 'true' ||
    normalized === 'up' ||
    normalized === 'active';
  return healthy
    ? 'bg-emerald-500/10 text-emerald-300 border-emerald-700/50'
    : 'bg-amber-500/10 text-amber-300 border-amber-700/50';
}

function buildNodeViews(contexts: WorkerContextTelemetry[]): WorkerNodeView[] {
  const views: WorkerNodeView[] = [];

  for (const context of contexts) {
    const fallbackNodes: WorkerSwarmNodeTelemetry[] =
      context.nodes.length > 0
        ? context.nodes
        : [
            {
              id: context.daemon?.id || context.name,
              hostname: context.daemon?.name || context.name,
              status: context.status === 'ok' ? 'Ready' : 'Unknown',
              availability: 'active',
              managerStatus: context.orchestrator || null,
              engineVersion: context.daemon?.serverVersion || '',
              tlsStatus: null,
              address: context.host,
              cpuCapacityCores: context.daemon?.cpuCount ?? null,
              memoryCapacityBytes: context.daemon?.memoryTotalBytes ?? null,
              containers: context.containers,
              aggregateLogs: '',
            },
          ];

    for (const node of fallbackNodes) {
      const identifiers = new Set(
        [node.id, node.hostname]
          .map((value) => String(value || '').trim())
          .filter((value) => value.length > 0)
      );

      let containers = Array.isArray(node.containers) ? node.containers : [];
      if (containers.length === 0) {
        containers = context.containers.filter((container) => {
          const nodeId = String(container.nodeId || '').trim();
          return nodeId.length > 0 && identifiers.has(nodeId);
        });
      }
      if (containers.length === 0 && fallbackNodes.length === 1) {
        containers = context.containers;
      }

      const key = `${context.name}:${node.id || node.hostname}`;
      views.push({
        key,
        context,
        node,
        containers,
        aggregateLogs: String(node.aggregateLogs || ''),
      });
    }
  }

  views.sort((left, right) => {
    if (left.context.current !== right.context.current) {
      return left.context.current ? -1 : 1;
    }
    return (left.node.hostname || left.node.id).localeCompare(
      right.node.hostname || right.node.id
    );
  });

  return views;
}

function UtilizationCard({
  title,
  usageText,
  percentText,
  percentValue,
}: {
  title: string;
  usageText: string;
  percentText: string;
  percentValue: number | null | undefined;
}) {
  const normalizedPercent = Number.isFinite(percentValue)
    ? Math.max(0, Math.min(100, Number(percentValue)))
    : 0;

  return (
    <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
      <div className="text-xs uppercase tracking-wide text-slate-400">{title}</div>
      <div className="mt-1 font-mono text-sm text-slate-200">{usageText}</div>
      <div className="mt-1 text-xs text-slate-500">{percentText}</div>
      <div className="mt-2 h-2 overflow-hidden rounded-full bg-slate-800">
        <div
          className="h-full rounded-full bg-sky-500/70"
          style={{ width: `${normalizedPercent}%` }}
        />
      </div>
    </div>
  );
}

function WorkerContainerList({
  containers,
  selectedContainerKey,
  onSelectContainer,
  resolveShardId,
}: {
  containers: WorkerContainerTelemetry[];
  selectedContainerKey: string | null;
  onSelectContainer: (containerKey: string) => void;
  resolveShardId: (container: WorkerContainerTelemetry) => string | null;
}) {
  return (
    <div className="overflow-hidden rounded-2xl border border-slate-800 bg-slate-900/70">
      <div className="border-b border-slate-800 px-4 py-3 text-sm font-semibold uppercase tracking-wide text-slate-400">
        Containers On Worker
      </div>
      {containers.length === 0 ? (
        <div className="px-4 py-4 text-sm text-slate-500">
          No containers found for this worker.
        </div>
      ) : (
        <div className="max-h-[420px] space-y-2 overflow-auto p-2">
          {containers.map((container, index) => {
            const key = containerSelectionKey(container, index);
            const isSelected = selectedContainerKey === key;
            const title = container.name || container.id || `container-${index + 1}`;
            const stateLabel = container.state || 'unknown';
            const shardId = resolveShardId(container);
            const hasCpuMetrics =
              Number.isFinite(container.cpuUsageCores) || Number.isFinite(container.cpuUsagePct);
            const hasMemoryMetrics =
              Number.isFinite(container.memoryUsageBytes) ||
              Number.isFinite(container.memoryUsagePct);

            return (
              <div key={key}>
                <button
                  type="button"
                  onClick={() => onSelectContainer(key)}
                  className={
                    'w-full rounded-lg border px-3 py-2 text-left transition-colors ' +
                    (isSelected
                      ? 'rounded-b-none border-sky-600 bg-sky-950/35 text-sky-100'
                      : 'border-slate-700 bg-slate-950/75 text-slate-300 hover:border-sky-800/60 hover:bg-slate-900')
                  }
                >
                  <div className="flex items-center justify-between gap-2">
                    <span className="truncate font-mono text-xs" title={title}>
                      {title}
                    </span>
                    <span className="rounded-full border border-slate-600 px-2 py-0.5 text-[10px] uppercase tracking-wide text-slate-300">
                      {stateLabel}
                    </span>
                  </div>
                  <div className="mt-1 truncate text-[11px] text-slate-500" title={container.image || '-'}>
                    {container.image || '-'}
                  </div>
                  <div className="mt-1 truncate text-[11px] text-slate-500" title={container.status || '-'}>
                    {container.status || '-'}
                  </div>
                </button>

                {isSelected && (
                  <div className="rounded-b-lg border border-t-0 border-sky-700/70 bg-slate-950/85 px-3 py-3 text-xs text-slate-300">
                    <div className="mb-2 rounded border border-slate-700 bg-slate-900/60 px-2 py-2">
                      <div className="text-[11px] uppercase tracking-wide text-slate-500">
                        Shard ID
                      </div>
                      <div className="break-all font-mono text-slate-200" title={shardId || '-'}>
                        {shardId || '-'}
                      </div>
                    </div>
                    <div className="grid grid-cols-1 gap-2 md:grid-cols-2">
                      <div>
                        <div className="text-[11px] uppercase tracking-wide text-slate-500">
                          Running For
                        </div>
                        <div className="font-mono text-slate-200">{container.runningFor || '-'}</div>
                      </div>
                      <div>
                        <div className="text-[11px] uppercase tracking-wide text-slate-500">
                          Created At
                        </div>
                        <div className="truncate text-slate-200" title={container.createdAt || '-'}>
                          {container.createdAt || '-'}
                        </div>
                      </div>
                      <div>
                        <div className="text-[11px] uppercase tracking-wide text-slate-500">Ports</div>
                        <div className="font-mono text-slate-200" title={container.ports || '-'}>
                          {container.ports || '-'}
                        </div>
                      </div>
                      <div>
                        <div className="text-[11px] uppercase tracking-wide text-slate-500">ID</div>
                        <div className="break-all font-mono text-slate-200" title={container.id || '-'}>
                          {container.id || '-'}
                        </div>
                      </div>
                    </div>

                    {container.command && (
                      <div className="mt-3">
                        <div className="text-[11px] uppercase tracking-wide text-slate-500">Command</div>
                        <div className="break-words font-mono text-slate-200" title={container.command}>
                          {container.command}
                        </div>
                      </div>
                    )}

                    {(hasCpuMetrics || hasMemoryMetrics) && (
                      <div className="mt-3 grid grid-cols-1 gap-2 md:grid-cols-2">
                        <div className="rounded border border-slate-700 bg-slate-900/60 px-2 py-2">
                          <div className="text-[11px] uppercase tracking-wide text-slate-500">CPU</div>
                          <div className="font-mono text-slate-200">
                            {formatCpuUsageValues(container.cpuUsageCores, container.cpuCapacityCores)}
                          </div>
                          <div className="text-[11px] text-slate-500">{formatPct(container.cpuUsagePct)}</div>
                        </div>
                        <div className="rounded border border-slate-700 bg-slate-900/60 px-2 py-2">
                          <div className="text-[11px] uppercase tracking-wide text-slate-500">Memory</div>
                          <div className="font-mono text-slate-200">
                            {formatMemoryUsageValues(
                              container.memoryUsageBytes,
                              container.memoryCapacityBytes
                            )}
                          </div>
                          <div className="text-[11px] text-slate-500">
                            {formatPct(container.memoryUsagePct)}
                          </div>
                        </div>
                      </div>
                    )}
                  </div>
                )}
              </div>
            );
          })}
        </div>
      )}
    </div>
  );
}

export default function WorkersPage() {
  const [pollIntervalMs, setPollIntervalMs] = useState(DEFAULT_POLL_INTERVAL_MS);
  const [selectedNodeKey, setSelectedNodeKey] = useState<string | null>(null);
  const [selectedContainerByWorker, setSelectedContainerByWorker] = useState<Record<string, string>>({});
  const telemetryPollIntervalMs =
    pollIntervalMs >= POLL_DISABLED_AT_MS ? 0 : pollIntervalMs;

  const snapshot = useWorkersSnapshot({
    intervalMs: telemetryPollIntervalMs,
    enabled: true,
    resetOnException: false,
    resetOnHttpError: false,
  });
  const shardPlacement = useShardPlacement({
    intervalMs:
      telemetryPollIntervalMs === 0 ? 0 : Math.max(1000, telemetryPollIntervalMs),
    enabled: true,
    resetOnException: false,
    resetOnHttpError: false,
  });

  const contexts = useMemo(() => {
    return [...snapshot.contexts].sort((left, right) => {
      if (left.current !== right.current) {
        return left.current ? -1 : 1;
      }
      return left.name.localeCompare(right.name);
    });
  }, [snapshot.contexts]);

  const workerNodes = useMemo(() => buildNodeViews(contexts), [contexts]);
  const shardIdByPodName = useMemo(
    () => buildShardIdByPodName(shardPlacement),
    [shardPlacement]
  );

  useEffect(() => {
    if (workerNodes.length === 0) {
      setSelectedNodeKey(null);
      return;
    }
    if (!selectedNodeKey || !workerNodes.some((node) => node.key === selectedNodeKey)) {
      setSelectedNodeKey(workerNodes[0].key);
    }
  }, [selectedNodeKey, workerNodes]);

  const selectedWorker = useMemo(() => {
    if (!selectedNodeKey) {
      return workerNodes[0] ?? null;
    }
    return workerNodes.find((node) => node.key === selectedNodeKey) ?? workerNodes[0] ?? null;
  }, [selectedNodeKey, workerNodes]);

  useEffect(() => {
    if (!selectedWorker) {
      return;
    }

    setSelectedContainerByWorker((previous) => {
      const selectedContainerKey = previous[selectedWorker.key];
      if (!selectedContainerKey) {
        return previous;
      }

      const stillExists = selectedWorker.containers.some(
        (container, index) => containerSelectionKey(container, index) === selectedContainerKey
      );
      if (stillExists) {
        return previous;
      }

      const next = { ...previous };
      delete next[selectedWorker.key];
      return next;
    });
  }, [selectedWorker]);

  const selectedContainerKey = selectedWorker
    ? selectedContainerByWorker[selectedWorker.key] ?? null
    : null;

  const selectedContainer = useMemo(() => {
    if (!selectedWorker || !selectedContainerKey) {
      return null;
    }
    return (
      selectedWorker.containers.find(
        (container, index) =>
          containerSelectionKey(container, index) === selectedContainerKey
      ) ?? null
    );
  }, [selectedContainerKey, selectedWorker]);

  const aggregateLogsBySource = useMemo(
    () => parseAggregateLogsBySource(selectedWorker?.aggregateLogs || ''),
    [selectedWorker?.aggregateLogs]
  );

  const selectedContainerLogs = useMemo(() => {
    if (!selectedContainer) {
      return '';
    }
    return resolveContainerLogs(selectedContainer, aggregateLogsBySource);
  }, [aggregateLogsBySource, selectedContainer]);

  const totalContainers = useMemo(
    () => workerNodes.reduce((sum, node) => sum + node.containers.length, 0),
    [workerNodes]
  );
  const onlineContexts = contexts.filter((context) => context.status === 'ok').length;

  const cpuUsageCores = selectedWorker
    ? pickMetricValue(selectedContainer?.cpuUsageCores, selectedWorker.node.cpuUsageCores)
    : null;
  const cpuCapacityCores = selectedWorker
    ? pickMetricValue(selectedContainer?.cpuCapacityCores, selectedWorker.node.cpuCapacityCores)
    : null;
  const cpuUsagePct = selectedWorker
    ? pickMetricValue(selectedContainer?.cpuUsagePct, selectedWorker.node.cpuUsagePct)
    : null;

  const memoryUsageBytes = selectedWorker
    ? pickMetricValue(selectedContainer?.memoryUsageBytes, selectedWorker.node.memoryUsageBytes)
    : null;
  const memoryCapacityBytes = selectedWorker
    ? pickMetricValue(
        selectedContainer?.memoryCapacityBytes,
        selectedWorker.node.memoryCapacityBytes
      )
    : null;
  const memoryUsagePct = selectedWorker
    ? pickMetricValue(selectedContainer?.memoryUsagePct, selectedWorker.node.memoryUsagePct)
    : null;

  const useContainerCpuUtilization = hasContainerCpuMetrics(selectedContainer);
  const useContainerMemoryUtilization = hasContainerMemoryMetrics(selectedContainer);

  const logsTitle = selectedContainer
    ? `Container Logs (${selectedContainer.name || selectedContainer.id || 'selected'})`
    : 'Aggregate Container Logs';
  const visibleLogs = selectedContainer
    ? selectedContainerLogs
    : selectedWorker?.aggregateLogs || '';
  const emptyLogsText = selectedContainer
    ? 'No logs collected for this container yet.'
    : 'No logs collected for this worker yet.';

  function onSelectContainer(containerKey: string): void {
    if (!selectedWorker) {
      return;
    }

    setSelectedContainerByWorker((previous) => {
      const current = previous[selectedWorker.key] ?? null;
      const next = { ...previous };

      if (current === containerKey) {
        delete next[selectedWorker.key];
      } else {
        next[selectedWorker.key] = containerKey;
      }

      return next;
    });
  }

  function resolveShardId(container: WorkerContainerTelemetry): string | null {
    const podName = extractPodName(container);
    if (!podName) {
      return null;
    }
    return shardIdByPodName.get(podName) || null;
  }

  return (
    <div className="grid grid-cols-1 gap-5 lg:grid-cols-[280px_minmax(0,1fr)]">
      <aside className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
        <div className="mb-2 text-xs uppercase tracking-wide text-slate-500">Workers (Nodes)</div>
        <div className="space-y-2">
          {workerNodes.map((workerNode) => {
            const nodeTitle = workerNode.node.hostname || workerNode.node.id || 'unknown-node';
            const subTitle = `${workerNode.context.name} • ${workerNode.containers.length} containers`;
            return (
              <button
                key={workerNode.key}
                type="button"
                onClick={() => setSelectedNodeKey(workerNode.key)}
                className={
                  'w-full rounded-lg border px-3 py-2 text-left text-sm ' +
                  (workerNode.key === selectedWorker?.key
                    ? 'border-sky-600 bg-sky-900/30 text-sky-100'
                    : 'border-slate-700 bg-slate-950 text-slate-300 hover:bg-slate-900')
                }
              >
                <div className="flex items-center justify-between gap-2">
                  <span className="truncate font-mono text-xs">{nodeTitle}</span>
                  <span
                    className={
                      'rounded-full border px-2 py-0.5 text-[10px] uppercase tracking-wide ' +
                      nodeStatusPillClasses(workerNode.node)
                    }
                  >
                    {workerNode.node.status || 'unknown'}
                  </span>
                </div>
                <div className="mt-1 truncate text-[11px] text-slate-500" title={subTitle}>
                  {subTitle}
                </div>
              </button>
            );
          })}
          {workerNodes.length === 0 && (
            <div className="rounded border border-slate-700 bg-slate-950 px-2 py-2 text-xs text-slate-500">
              No worker nodes detected.
            </div>
          )}
        </div>
      </aside>

      <section className="space-y-5">
        <div>
          <h1 className="text-2xl font-semibold text-slate-100">Workers</h1>
          <p className="mt-1 text-sm text-slate-400">
            Node-specific worker state with containers, logs, and utilization.
          </p>
          <p className="mt-1 text-xs text-slate-500">
            Last updated: {formatUpdatedAt(snapshot.collectedAtMs)}
          </p>
        </div>

        <div className="grid grid-cols-2 gap-3 md:grid-cols-4">
          <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
            <div className="text-xs text-slate-400">Contexts</div>
            <div className="font-mono text-xl text-slate-100">{contexts.length}</div>
          </div>
          <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
            <div className="text-xs text-slate-400">Reachable</div>
            <div className="font-mono text-xl text-slate-100">{onlineContexts}</div>
          </div>
          <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
            <div className="text-xs text-slate-400">Worker Nodes</div>
            <div className="font-mono text-xl text-slate-100">{workerNodes.length}</div>
          </div>
          <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
            <div className="text-xs text-slate-400">Containers</div>
            <div className="font-mono text-xl text-slate-100">{totalContainers}</div>
          </div>
        </div>

        <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
          <label className="flex flex-wrap items-center gap-2 text-xs text-slate-400">
            <span>poll interval:</span>
            <input
              type="range"
              min={MIN_POLL_INTERVAL_MS}
              max={MAX_POLL_INTERVAL_MS}
              step={POLL_STEP_MS}
              value={pollIntervalMs}
              onChange={(event) =>
                setPollIntervalMs(
                  Math.max(
                    MIN_POLL_INTERVAL_MS,
                    Math.min(MAX_POLL_INTERVAL_MS, Number(event.target.value))
                  )
                )
              }
            />
            <span>
              {pollIntervalMs >= POLL_DISABLED_AT_MS
                ? 'off'
                : `${pollIntervalMs}ms`}
            </span>
          </label>
        </div>

        {snapshot.error && (
          <div className="rounded-xl border border-rose-900 bg-rose-950/40 px-3 py-2 text-sm text-rose-200">
            Worker snapshot error: {snapshot.error}
          </div>
        )}

        {selectedWorker ? (
          <>
            <div className="grid grid-cols-1 gap-3 md:grid-cols-2">
              <UtilizationCard
                title={
                  useContainerCpuUtilization
                    ? 'CPU Utilization (Container)'
                    : 'CPU Utilization (Worker)'
                }
                usageText={formatCpuUsageValues(cpuUsageCores, cpuCapacityCores)}
                percentText={formatPct(cpuUsagePct)}
                percentValue={cpuUsagePct}
              />
              <UtilizationCard
                title={
                  useContainerMemoryUtilization
                    ? 'Memory Utilization (Container)'
                    : 'Memory Utilization (Worker)'
                }
                usageText={formatMemoryUsageValues(memoryUsageBytes, memoryCapacityBytes)}
                percentText={formatPct(memoryUsagePct)}
                percentValue={memoryUsagePct}
              />
            </div>

            <WorkerContainerList
              containers={selectedWorker.containers}
              selectedContainerKey={selectedContainerKey}
              onSelectContainer={onSelectContainer}
              resolveShardId={resolveShardId}
            />

            <div className="overflow-hidden rounded-2xl border border-slate-800 bg-slate-900/70">
              <div className="border-b border-slate-800 px-4 py-3 text-sm font-semibold uppercase tracking-wide text-slate-400">
                {logsTitle}
              </div>
              {visibleLogs ? (
                <pre className="max-h-[420px] overflow-auto whitespace-pre-wrap break-words px-4 py-3 font-mono text-sm leading-6 text-slate-200">
                  {visibleLogs}
                </pre>
              ) : (
                <div className="px-4 py-4 text-sm text-slate-500">{emptyLogsText}</div>
              )}
            </div>
          </>
        ) : (
          <div className="rounded-xl border border-slate-800 bg-slate-900/70 px-3 py-3 text-sm text-slate-500">
            Select a worker node from the sidebar.
          </div>
        )}
      </section>
    </div>
  );
}
