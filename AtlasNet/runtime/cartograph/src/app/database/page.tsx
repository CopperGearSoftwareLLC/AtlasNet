'use client';

import { useEffect, useMemo, useRef, useState } from 'react';
import type {
  DatabaseRecord,
  DatabaseSnapshotResponse,
  DatabaseSource,
} from '../lib/cartographTypes';
import { HardcodedDecodeToggle } from './RevertByteOptimation/HardcodedDecodeToggle';
import { DatabaseExplorerInspector } from './components/DatabaseExplorerInspector';

const DEFAULT_POLL_INTERVAL_MS = 1000;
const MIN_POLL_INTERVAL_MS = 250;
const MAX_POLL_INTERVAL_MS = 5000;
const POLL_DISABLED_AT_MS = MAX_POLL_INTERVAL_MS;

export default function DatabasePage() {
  const [sources, setSources] = useState<DatabaseSource[]>([]);
  const [selectedSource, setSelectedSource] = useState<string | null>(null);
  const [records, setRecords] = useState<DatabaseRecord[]>([]);
  const [decodeSerialized, setDecodeSerialized] = useState(true);
  const [pollIntervalMs, setPollIntervalMs] = useState(DEFAULT_POLL_INTERVAL_MS);
  const [refreshToken, setRefreshToken] = useState(0);
  const [loading, setLoading] = useState(true);
  const [errorText, setErrorText] = useState<string | null>(null);
  const [lastUpdatedMs, setLastUpdatedMs] = useState<number | null>(null);
  const decodeSerializedRef = useRef(decodeSerialized);

  useEffect(() => {
    decodeSerializedRef.current = decodeSerialized;
  }, [decodeSerialized]);

  useEffect(() => {
    let alive = true;

    async function poll() {
      try {
        const params = new URLSearchParams();
        if (selectedSource) {
          params.set('source', selectedSource);
        }
        params.set('decodeSerialized', decodeSerializedRef.current ? '1' : '0');
        const response = await fetch(`/api/databases?${params.toString()}`, {
          cache: 'no-store',
        });
        if (!response.ok) {
          if (!alive) return;
          setErrorText('Database endpoint returned a non-200 response.');
          return;
        }

        const payload = (await response.json()) as DatabaseSnapshotResponse;
        if (!alive) return;

        const nextSources = Array.isArray(payload.sources)
          ? payload.sources
              .filter((source) => source.running)
              .sort((a, b) => a.name.localeCompare(b.name))
          : [];
        const nextRecords = Array.isArray(payload.records) ? payload.records : [];

        setSources(nextSources);
        setRecords(nextRecords);
        setLastUpdatedMs(Date.now());
        setErrorText(null);

        if (payload.selectedSource && payload.selectedSource !== selectedSource) {
          setSelectedSource(payload.selectedSource);
        }
        if (!payload.selectedSource && nextSources.length > 0 && !selectedSource) {
          setSelectedSource(nextSources[0].id);
        }
        if (!payload.selectedSource && nextSources.length === 0 && selectedSource) {
          setSelectedSource(null);
        }
      } catch {
        if (!alive) return;
        setSources([]);
        setRecords([]);
        setErrorText('Failed to fetch database snapshot.');
      } finally {
        if (!alive) return;
        setLoading(false);
      }
    }

    void poll();
    const intervalId =
      pollIntervalMs >= POLL_DISABLED_AT_MS
        ? null
        : setInterval(() => {
            void poll();
          }, pollIntervalMs);

    return () => {
      alive = false;
      if (intervalId != null) {
        clearInterval(intervalId);
      }
    };
  }, [pollIntervalMs, refreshToken, selectedSource]);

  const selectedSourceView = useMemo(
    () => sources.find((source) => source.id === selectedSource) ?? null,
    [selectedSource, sources]
  );

  return (
    <div className="grid grid-cols-1 gap-5 lg:grid-cols-[220px_minmax(0,1fr)]">
      <aside className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
        <div className="mb-2 text-xs uppercase tracking-wide text-slate-500">
          Active Databases
        </div>
        <div className="space-y-2">
          {sources.map((source) => (
            <button
              key={source.id}
              type="button"
              onClick={() => setSelectedSource(source.id)}
              className={
                'w-full rounded-lg border px-3 py-2 text-left text-sm ' +
                (source.id === selectedSource
                  ? 'border-sky-600 bg-sky-900/30 text-sky-200'
                  : 'border-slate-700 bg-slate-950 text-slate-300 hover:bg-slate-900')
              }
            >
              <div className="font-mono text-xs">{source.name}</div>
              <div className="font-mono text-[11px] text-slate-500">
                {source.latencyMs == null ? '-' : `${source.latencyMs}ms`}
              </div>
            </button>
          ))}
          {!loading && sources.length === 0 && (
            <div className="rounded border border-slate-700 bg-slate-950 px-2 py-2 text-xs text-slate-500">
              No running DBs
            </div>
          )}
        </div>
      </aside>

      <section className="space-y-5">
        <div className="flex flex-wrap items-start justify-between gap-3">
          <div>
            <h1 className="text-2xl font-semibold text-slate-100">Database</h1>
            <p className="mt-1 text-sm text-slate-400">
              {selectedSourceView
                ? `Viewing ${selectedSourceView.name} (${selectedSourceView.host}:${selectedSourceView.port})`
                : 'Select an active database from the sidebar.'}
            </p>
            <p className="mt-1 text-xs text-slate-500">
              {lastUpdatedMs
                ? `Last updated ${new Date(lastUpdatedMs).toLocaleTimeString()}`
                : 'Waiting for first snapshot...'}
            </p>
          </div>

          <button
            type="button"
            onClick={() => setRefreshToken((value) => value + 1)}
            className="rounded-xl border border-slate-700 bg-slate-900 px-3 py-2 text-sm text-slate-200 hover:bg-slate-800"
          >
            Refresh now
          </button>
        </div>

        <div className="grid grid-cols-2 gap-3 md:grid-cols-3">
          <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
            <div className="text-xs text-slate-400">Total Keys</div>
            <div className="font-mono text-xl text-slate-100">{records.length}</div>
          </div>
          <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
            <div className="text-xs text-slate-400">Active DBs</div>
            <div className="font-mono text-xl text-slate-100">{sources.length}</div>
          </div>
          <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
            <div className="text-xs text-slate-400">Poll Interval</div>
            <div className="font-mono text-xl text-slate-100">
              {pollIntervalMs >= POLL_DISABLED_AT_MS ? 'off' : `${pollIntervalMs}ms`}
            </div>
          </div>
        </div>

        <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
          <div className="flex flex-wrap items-center gap-3">
            <HardcodedDecodeToggle enabled={decodeSerialized} onChange={setDecodeSerialized} />
            <label className="flex min-w-60 items-center gap-2 text-xs text-slate-400">
              <input
                type="range"
                min={MIN_POLL_INTERVAL_MS}
                max={MAX_POLL_INTERVAL_MS}
                step={250}
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
              poll interval:{' '}
              {pollIntervalMs >= POLL_DISABLED_AT_MS ? 'off' : `${pollIntervalMs}ms`}
            </label>
          </div>
        </div>

        {errorText && (
          <div className="rounded-xl border border-rose-900 bg-rose-950/40 px-3 py-2 text-sm text-rose-200">
            {errorText}
          </div>
        )}

        <DatabaseExplorerInspector
          records={records}
          loading={loading}
          selectionScopeKey={selectedSource}
        />
      </section>
    </div>
  );
}
