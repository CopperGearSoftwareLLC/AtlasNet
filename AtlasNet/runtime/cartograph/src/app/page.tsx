'use client';

import Link from 'next/link';
import { useEffect, useMemo, useState } from 'react';
import { ServerBoundsMinimapSection } from './components/ServerBoundsMinimapSection';
import type { DatabaseSnapshotResponse } from './lib/cartographTypes';
import {
  useAuthorityEntities,
  useHeuristicShapes,
  useNetworkTelemetry,
  useWorkersSnapshot,
} from './lib/hooks/useTelemetryFeeds';
import { useServerBoundsMinimapData } from './lib/hooks/useServerBoundsMinimapData';

const TELEMETRY_POLL_INTERVAL_MS = 1000;
const WORKERS_POLL_INTERVAL_MS = 5000;
const DATABASE_POLL_INTERVAL_MS = 5000;

interface DatabaseSummary {
  activeSources: number;
  totalKeys: number;
  heuristicType: string | null;
  lastUpdatedMs: number | null;
  errorText: string | null;
}

interface HeuristicTypeResponse {
  heuristicType: string | null;
}

const EMPTY_DATABASE_SUMMARY: DatabaseSummary = {
  activeSources: 0,
  totalKeys: 0,
  heuristicType: null,
  lastUpdatedMs: null,
  errorText: null,
};

function parseHeuristicType(records: DatabaseSnapshotResponse['records']): string | null {
  if (!Array.isArray(records)) {
    return null;
  }

  const heuristicManifestRecord = records.find(
    (record) => String(record.key ?? '').trim().toLowerCase() === 'heuristicmanifest'
  );
  if (heuristicManifestRecord) {
    const payload = String(heuristicManifestRecord.payload ?? '').trim();
    if (payload) {
      try {
        let parsed = JSON.parse(payload) as unknown;
        if (Array.isArray(parsed)) {
          parsed = parsed[0];
        }
        if (
          parsed &&
          typeof parsed === 'object' &&
          typeof (parsed as Record<string, unknown>).HeuristicType === 'string'
        ) {
          const text = String(
            (parsed as Record<string, unknown>).HeuristicType
          ).trim();
          if (text.length > 0) {
            return text;
          }
        }
      } catch {}
    }
  }

  // Backward-compat fallback for legacy key.
  const heuristicRecord = records.find(
    (record) => String(record.key ?? '').trim().toLowerCase() === 'heuristic_type'
  );
  if (!heuristicRecord) {
    return null;
  }

  const payload = String(heuristicRecord.payload ?? '').trim();
  if (!payload) {
    return null;
  }

  try {
    const parsed = JSON.parse(payload) as unknown;
    if (typeof parsed === 'string') {
      const text = parsed.trim();
      return text.length > 0 ? text : null;
    }
  } catch {}

  const firstLine = payload.split(/\r?\n/)[0]?.trim();
  return firstLine && firstLine.length > 0 ? firstLine : null;
}

function MetricCard({
  label,
  value,
  hint,
}: {
  label: string;
  value: string | number;
  hint: string;
}) {
  return (
    <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
      <div className="text-xs uppercase tracking-wide text-slate-400">{label}</div>
      <div className="font-mono text-2xl text-slate-100">{value}</div>
      <div className="text-xs text-slate-500">{hint}</div>
    </div>
  );
}

export default function OverviewPage() {
  const heuristicShapes = useHeuristicShapes({
    intervalMs: TELEMETRY_POLL_INTERVAL_MS,
    enabled: true,
    resetOnException: true,
    resetOnHttpError: false,
  });
  const authorityEntities = useAuthorityEntities({
    intervalMs: TELEMETRY_POLL_INTERVAL_MS,
    enabled: true,
    resetOnException: true,
    resetOnHttpError: false,
  });
  const networkTelemetry = useNetworkTelemetry({
    intervalMs: TELEMETRY_POLL_INTERVAL_MS,
    enabled: true,
    resetOnException: true,
    resetOnHttpError: false,
  });
  const workersSnapshot = useWorkersSnapshot({
    intervalMs: WORKERS_POLL_INTERVAL_MS,
    enabled: true,
    resetOnException: false,
    resetOnHttpError: false,
  });

  const {
    shardSummaries,
    shardBoundsByIdWithNetworkFallback,
    claimedBoundShardIds,
    networkNodeIdSet,
    networkEdgeCount,
  } = useServerBoundsMinimapData({
    heuristicShapes,
    authorityEntities,
    networkTelemetry,
  });

  const [databaseSummary, setDatabaseSummary] =
    useState<DatabaseSummary>(EMPTY_DATABASE_SUMMARY);

  useEffect(() => {
    let alive = true;

    async function pollDatabaseSummary() {
      try {
        const response = await fetch('/api/databases?decodeSerialized=0', {
          cache: 'no-store',
        });
        if (!response.ok) {
          if (!alive) {
            return;
          }
          setDatabaseSummary((prev) => ({
            ...prev,
            errorText: `Database endpoint returned ${response.status}.`,
          }));
          return;
        }

        const payload = (await response.json()) as DatabaseSnapshotResponse;
        if (!alive) {
          return;
        }

        let heuristicType: string | null = null;
        try {
          const heuristicTypeResponse = await fetch('/api/heuristictype', {
            cache: 'no-store',
          });
          if (heuristicTypeResponse.ok) {
            const heuristicTypePayload =
              (await heuristicTypeResponse.json()) as HeuristicTypeResponse;
            if (typeof heuristicTypePayload.heuristicType === 'string') {
              const text = heuristicTypePayload.heuristicType.trim();
              heuristicType = text.length > 0 ? text : null;
            }
          }
        } catch {}

        const sources = Array.isArray(payload.sources)
          ? payload.sources.filter((source) => source.running)
          : [];
        const records = Array.isArray(payload.records) ? payload.records : [];
        setDatabaseSummary({
          activeSources: sources.length,
          totalKeys: records.length,
          heuristicType: heuristicType ?? parseHeuristicType(records),
          lastUpdatedMs: Date.now(),
          errorText: null,
        });
      } catch {
        if (!alive) {
          return;
        }
        setDatabaseSummary((prev) => ({
          ...prev,
          errorText: 'Database summary fetch failed.',
        }));
      }
    }

    void pollDatabaseSummary();
    const intervalId = setInterval(() => {
      void pollDatabaseSummary();
    }, DATABASE_POLL_INTERVAL_MS);

    return () => {
      alive = false;
      clearInterval(intervalId);
    };
  }, []);

  const uniqueClientCount = useMemo(() => {
    const keys = new Set<string>();
    for (const entity of authorityEntities) {
      if (!entity.isClient) {
        continue;
      }
      const key = entity.clientId.trim() || `entity:${entity.entityId}`;
      keys.add(key);
    }
    return keys.size;
  }, [authorityEntities]);

  const contextCount = workersSnapshot.contexts.length;
  const reachableContextCount = workersSnapshot.contexts.filter(
    (context) => context.status === 'ok'
  ).length;

  return (
    <div className="space-y-6">
      <div className="flex flex-wrap items-end justify-between gap-3">
        <div>
          <h1 className="text-2xl font-semibold text-slate-100">Overview</h1>
          <p className="mt-1 text-sm text-slate-400">
            High-level status across map, clients, workers, network, and database.
          </p>
        </div>
        <div className="flex flex-wrap gap-2 text-xs">
          <Link
            href="/map"
            className="rounded-lg border border-slate-700 bg-slate-900 px-3 py-2 text-slate-200 hover:bg-slate-800"
          >
            Open Map
          </Link>
          <Link
            href="/clients"
            className="rounded-lg border border-slate-700 bg-slate-900 px-3 py-2 text-slate-200 hover:bg-slate-800"
          >
            Open Clients
          </Link>
          <Link
            href="/workers"
            className="rounded-lg border border-slate-700 bg-slate-900 px-3 py-2 text-slate-200 hover:bg-slate-800"
          >
            Open Workers
          </Link>
          <Link
            href="/database"
            className="rounded-lg border border-slate-700 bg-slate-900 px-3 py-2 text-slate-200 hover:bg-slate-800"
          >
            Open Database
          </Link>
          <Link
            href="/network"
            className="rounded-lg border border-slate-700 bg-slate-900 px-3 py-2 text-slate-200 hover:bg-slate-800"
          >
            Open Network
          </Link>
        </div>
      </div>

      <section className="grid grid-cols-2 gap-3 md:grid-cols-3 xl:grid-cols-5">
        <MetricCard
          label="Clients"
          value={uniqueClientCount}
          hint={`${authorityEntities.length} tracked entities`}
        />
        <MetricCard
          label="Worker Contexts"
          value={contextCount}
          hint={`${reachableContextCount} reachable`}
        />
        <MetricCard
          label="Shards"
          value={shardSummaries.length}
          hint={`${networkNodeIdSet.size} in telemetry`}
        />
        <MetricCard
          label="Network Links"
          value={networkEdgeCount}
          hint="deduplicated GNS edges"
        />
        <MetricCard
          label="Curr Bounds Heuristic"
          value={databaseSummary.heuristicType ?? 'unknown'}
          hint="from HeuristicManifest.HeuristicType"
        />
      </section>

      <section className="grid grid-cols-1 gap-4 xl:grid-cols-2">
        <article className="rounded-2xl border border-slate-800 bg-slate-900/70 p-4">
          <h2 className="text-sm font-semibold uppercase tracking-wide text-slate-400">Workers</h2>
          <p className="mt-2 text-sm text-slate-300">
            {contextCount} contexts discovered, {reachableContextCount} currently reachable.
          </p>
          <p className="mt-1 text-xs text-slate-500">
            {workersSnapshot.error ?? 'Worker snapshot stream healthy.'}
          </p>
        </article>

        <article className="rounded-2xl border border-slate-800 bg-slate-900/70 p-4">
          <h2 className="text-sm font-semibold uppercase tracking-wide text-slate-400">Database</h2>
          <p className="mt-2 text-sm text-slate-300">
            {databaseSummary.activeSources} active sources, {databaseSummary.totalKeys} keys loaded.
          </p>
          <p className="mt-1 text-xs text-slate-500">
            {databaseSummary.errorText ??
              (databaseSummary.lastUpdatedMs
                ? `Updated ${new Date(databaseSummary.lastUpdatedMs).toLocaleTimeString()}`
                : 'Waiting for first DB snapshot...')}
          </p>
        </article>
      </section>

      <ServerBoundsMinimapSection
        shardSummaries={shardSummaries}
        boundsByShardId={shardBoundsByIdWithNetworkFallback}
        claimedBoundShardIds={claimedBoundShardIds}
      />
    </div>
  );
}
