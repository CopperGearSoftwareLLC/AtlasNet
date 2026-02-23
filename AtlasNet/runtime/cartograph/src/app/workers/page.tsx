'use client';

import { useEffect, useMemo, useState } from 'react';
import type {
  WorkerContainerTelemetry,
  WorkerContextTelemetry,
  WorkerDaemonTelemetry,
  WorkerSwarmNodeTelemetry,
} from '../lib/cartographTypes';
import { useWorkersSnapshot } from '../lib/hooks/useTelemetryFeeds';

const DEFAULT_POLL_INTERVAL_MS = 5000;
const MIN_POLL_INTERVAL_MS = 0;
const MAX_POLL_INTERVAL_MS = 30000;
const POLL_STEP_MS = 1000;

function formatBytes(totalBytes: number): string {
  if (!Number.isFinite(totalBytes) || totalBytes <= 0) {
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

function statusPillClasses(status: WorkerContextTelemetry['status']): string {
  return status === 'ok'
    ? 'bg-emerald-500/10 text-emerald-300 border-emerald-700/50'
    : 'bg-rose-500/10 text-rose-200 border-rose-800/60';
}

function WorkerDetailRow({
  label,
  value,
}: {
  label: string;
  value: string;
}) {
  return (
    <>
      <dt className="text-slate-500">{label}</dt>
      <dd className="truncate font-mono text-slate-200" title={value}>
        {value || '-'}
      </dd>
    </>
  );
}

function WorkerDaemonCard({ daemon }: { daemon: WorkerDaemonTelemetry | null }) {
  if (!daemon) {
    return (
      <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-4 text-sm text-slate-400">
        Daemon details unavailable for this worker.
      </div>
    );
  }

  return (
    <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-4">
      <h2 className="mb-3 text-sm font-semibold uppercase tracking-wide text-slate-400">
        Docker Daemon
      </h2>
      <dl className="grid grid-cols-[140px_minmax(0,1fr)] gap-x-3 gap-y-2 text-sm">
        <WorkerDetailRow label="Name" value={daemon.name} />
        <WorkerDetailRow label="Version" value={daemon.serverVersion} />
        <WorkerDetailRow label="OS" value={daemon.operatingSystem} />
        <WorkerDetailRow label="Kernel" value={daemon.kernelVersion} />
        <WorkerDetailRow label="Arch" value={daemon.architecture} />
        <WorkerDetailRow label="CPUs" value={String(daemon.cpuCount)} />
        <WorkerDetailRow label="Memory" value={formatBytes(daemon.memoryTotalBytes)} />
        <WorkerDetailRow label="Swarm State" value={daemon.swarmState || 'inactive'} />
        <WorkerDetailRow
          label="Node Address"
          value={daemon.swarmNodeAddress || '-'}
        />
      </dl>
    </div>
  );
}

function WorkerNodesTable({ nodes }: { nodes: WorkerSwarmNodeTelemetry[] }) {
  return (
    <div className="overflow-hidden rounded-2xl border border-slate-800 bg-slate-900/70">
      <div className="border-b border-slate-800 px-4 py-3 text-sm font-semibold uppercase tracking-wide text-slate-400">
        Connected Nodes
      </div>
      {nodes.length === 0 ? (
        <div className="px-4 py-4 text-sm text-slate-500">No swarm node data reported.</div>
      ) : (
        <div className="overflow-x-auto">
          <table className="min-w-full text-left text-sm">
            <thead className="text-xs uppercase text-slate-500">
              <tr>
                <th className="px-4 py-3">Hostname</th>
                <th className="px-4 py-3">Status</th>
                <th className="px-4 py-3">Availability</th>
                <th className="px-4 py-3">Manager</th>
                <th className="px-4 py-3">Engine</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-slate-800 text-slate-200">
              {nodes.map((node) => (
                <tr key={`${node.id}:${node.hostname}`}>
                  <td className="px-4 py-2 font-mono">{node.hostname || node.id}</td>
                  <td className="px-4 py-2">{node.status || '-'}</td>
                  <td className="px-4 py-2">{node.availability || '-'}</td>
                  <td className="px-4 py-2">{node.managerStatus ?? '-'}</td>
                  <td className="px-4 py-2">{node.engineVersion || '-'}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
}

function WorkerContainersTable({
  containers,
}: {
  containers: WorkerContainerTelemetry[];
}) {
  return (
    <div className="overflow-hidden rounded-2xl border border-slate-800 bg-slate-900/70">
      <div className="border-b border-slate-800 px-4 py-3 text-sm font-semibold uppercase tracking-wide text-slate-400">
        Containers
      </div>
      {containers.length === 0 ? (
        <div className="px-4 py-4 text-sm text-slate-500">No containers found.</div>
      ) : (
        <div className="overflow-x-auto">
          <table className="min-w-full text-left text-sm">
            <thead className="text-xs uppercase text-slate-500">
              <tr>
                <th className="px-4 py-3">Name</th>
                <th className="px-4 py-3">Image</th>
                <th className="px-4 py-3">State</th>
                <th className="px-4 py-3">Status</th>
                <th className="px-4 py-3">Ports</th>
              </tr>
            </thead>
            <tbody className="divide-y divide-slate-800 text-slate-200">
              {containers.map((container) => (
                <tr key={container.id || `${container.name}:${container.image}`}>
                  <td className="max-w-56 truncate px-4 py-2 font-mono" title={container.name}>
                    {container.name || container.id || '-'}
                  </td>
                  <td className="max-w-64 truncate px-4 py-2" title={container.image}>
                    {container.image || '-'}
                  </td>
                  <td className="px-4 py-2">{container.state || '-'}</td>
                  <td className="max-w-64 truncate px-4 py-2" title={container.status}>
                    {container.status || '-'}
                  </td>
                  <td className="max-w-72 truncate px-4 py-2 font-mono" title={container.ports}>
                    {container.ports || '-'}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
}

export default function WorkersPage() {
  const [pollIntervalMs, setPollIntervalMs] = useState(DEFAULT_POLL_INTERVAL_MS);
  const [selectedContextName, setSelectedContextName] = useState<string | null>(null);

  const snapshot = useWorkersSnapshot({
    intervalMs: pollIntervalMs,
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

  useEffect(() => {
    if (contexts.length === 0) {
      setSelectedContextName(null);
      return;
    }
    if (!selectedContextName || !contexts.some((ctx) => ctx.name === selectedContextName)) {
      setSelectedContextName(contexts[0].name);
    }
  }, [contexts, selectedContextName]);

  const selectedContext = useMemo(() => {
    if (!selectedContextName) {
      return contexts[0] ?? null;
    }
    return contexts.find((ctx) => ctx.name === selectedContextName) ?? contexts[0] ?? null;
  }, [contexts, selectedContextName]);

  const onlineCount = contexts.filter((ctx) => ctx.status === 'ok').length;
  const totalNodes = contexts.reduce((sum, ctx) => sum + ctx.nodes.length, 0);
  const totalContainers = contexts.reduce((sum, ctx) => sum + ctx.containers.length, 0);

  return (
    <div className="grid grid-cols-1 gap-5 lg:grid-cols-[250px_minmax(0,1fr)]">
      <aside className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
        <div className="mb-2 text-xs uppercase tracking-wide text-slate-500">Workers</div>
        <div className="space-y-2">
          {contexts.map((context) => (
            <button
              key={context.name}
              type="button"
              onClick={() => setSelectedContextName(context.name)}
              className={
                'w-full rounded-lg border px-3 py-2 text-left text-sm ' +
                (context.name === selectedContext?.name
                  ? 'border-sky-600 bg-sky-900/30 text-sky-100'
                  : 'border-slate-700 bg-slate-950 text-slate-300 hover:bg-slate-900')
              }
            >
              <div className="flex items-center justify-between gap-2">
                <span className="truncate font-mono text-xs">{context.name}</span>
                <span
                  className={
                    'rounded-full border px-2 py-0.5 text-[10px] uppercase tracking-wide ' +
                    statusPillClasses(context.status)
                  }
                >
                  {context.status}
                </span>
              </div>
              <div className="mt-1 truncate text-[11px] text-slate-500" title={context.host ?? ''}>
                {(context.host ?? context.dockerEndpoint) || '-'}
              </div>
            </button>
          ))}
          {contexts.length === 0 && (
            <div className="rounded border border-slate-700 bg-slate-950 px-2 py-2 text-xs text-slate-500">
              No Docker workers detected.
            </div>
          )}
        </div>
      </aside>

      <section className="space-y-5">
        <div>
          <h1 className="text-2xl font-semibold text-slate-100">Workers</h1>
          <p className="mt-1 text-sm text-slate-400">
            Full Docker context, machine, and container details for connected workers.
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
            <div className="font-mono text-xl text-slate-100">{onlineCount}</div>
          </div>
          <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
            <div className="text-xs text-slate-400">Nodes</div>
            <div className="font-mono text-xl text-slate-100">{totalNodes}</div>
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
            <span>{pollIntervalMs <= 0 ? 'off' : `${pollIntervalMs}ms`}</span>
          </label>
        </div>

        {snapshot.error && (
          <div className="rounded-xl border border-rose-900 bg-rose-950/40 px-3 py-2 text-sm text-rose-200">
            Worker snapshot error: {snapshot.error}
          </div>
        )}
        {snapshot.collectedAtMs != null &&
          !snapshot.dockerCliAvailable &&
          snapshot.contexts.length === 0 && (
          <div className="rounded-xl border border-amber-900 bg-amber-950/40 px-3 py-2 text-sm text-amber-200">
            Docker CLI is not available in the Cartograph runtime environment.
          </div>
        )}

        {selectedContext ? (
          <div className="space-y-4">
            <div className="grid grid-cols-1 gap-4 xl:grid-cols-2">
              <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-4">
                <h2 className="mb-3 text-sm font-semibold uppercase tracking-wide text-slate-400">
                  Selected Worker
                </h2>
                <dl className="grid grid-cols-[140px_minmax(0,1fr)] gap-x-3 gap-y-2 text-sm">
                  <WorkerDetailRow label="Context" value={selectedContext.name} />
                  <WorkerDetailRow
                    label="Endpoint"
                    value={selectedContext.dockerEndpoint || '-'}
                  />
                  <WorkerDetailRow label="Host" value={selectedContext.host || '-'} />
                  <WorkerDetailRow
                    label="Current"
                    value={selectedContext.current ? 'yes' : 'no'}
                  />
                  <WorkerDetailRow
                    label="Orchestrator"
                    value={selectedContext.orchestrator || '-'}
                  />
                  <WorkerDetailRow label="Source" value={selectedContext.source || '-'} />
                  <WorkerDetailRow label="Status" value={selectedContext.status} />
                </dl>
              </div>

              <WorkerDaemonCard daemon={selectedContext.daemon} />
            </div>

            {selectedContext.error && (
              <div className="rounded-xl border border-rose-900 bg-rose-950/40 px-3 py-2 text-sm text-rose-200">
                Context error: {selectedContext.error}
              </div>
            )}

            <WorkerNodesTable nodes={selectedContext.nodes} />
            <WorkerContainersTable containers={selectedContext.containers} />
          </div>
        ) : (
          <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-4 text-sm text-slate-500">
            Select a worker context to inspect machine and container details.
          </div>
        )}
      </section>
    </div>
  );
}
