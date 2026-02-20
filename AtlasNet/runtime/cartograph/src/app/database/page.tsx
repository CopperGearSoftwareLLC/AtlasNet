'use client';

import { useEffect, useMemo, useState } from 'react';
import type { DatabaseRecord } from '../lib/databaseTypes';

const DEFAULT_POLL_INTERVAL_MS = 1000;
const MIN_POLL_INTERVAL_MS = 250;
const MAX_POLL_INTERVAL_MS = 5000;

function formatTtl(ttlSeconds: number): string {
  if (ttlSeconds === -1) return 'persistent';
  if (ttlSeconds === -2) return 'missing';
  return `${ttlSeconds}s`;
}

function payloadPreview(payload: string): string {
  const normalized = payload.replace(/\s+/g, ' ').trim();
  if (normalized.length === 0) {
    return '(empty)';
  }
  if (normalized.length <= 96) {
    return normalized;
  }
  return `${normalized.slice(0, 96)}...`;
}

export default function DatabasePage() {
  const [records, setRecords] = useState<DatabaseRecord[]>([]);
  const [search, setSearch] = useState('');
  const [typeFilter, setTypeFilter] = useState('all');
  const [expandedRowId, setExpandedRowId] = useState<string | null>(null);
  const [pollIntervalMs, setPollIntervalMs] = useState(DEFAULT_POLL_INTERVAL_MS);
  const [refreshToken, setRefreshToken] = useState(0);
  const [loading, setLoading] = useState(true);
  const [errorText, setErrorText] = useState<string | null>(null);
  const [lastUpdatedMs, setLastUpdatedMs] = useState<number | null>(null);

  useEffect(() => {
    let alive = true;

    async function poll() {
      try {
        const response = await fetch('/api/databases', { cache: 'no-store' });
        if (!response.ok) {
          if (!alive) return;
          setErrorText('Database endpoint returned a non-200 response.');
          return;
        }

        const payload = (await response.json()) as DatabaseRecord[];
        if (!alive) return;
        setRecords(Array.isArray(payload) ? payload : []);
        setLastUpdatedMs(Date.now());
        setErrorText(null);
      } catch {
        if (!alive) return;
        setErrorText('Failed to fetch database snapshot.');
      } finally {
        if (!alive) return;
        setLoading(false);
      }
    }

    poll();
    const intervalId = setInterval(poll, pollIntervalMs);

    return () => {
      alive = false;
      clearInterval(intervalId);
    };
  }, [pollIntervalMs, refreshToken]);

  const knownTypes = useMemo(() => {
    const values = new Set<string>();
    for (const record of records) {
      values.add(record.type || 'unknown');
    }
    return ['all', ...Array.from(values.values()).sort()];
  }, [records]);

  const filteredRecords = useMemo(() => {
    const query = search.trim().toLowerCase();

    return records
      .filter((record) => {
        if (typeFilter !== 'all' && record.type !== typeFilter) {
          return false;
        }

        if (query.length === 0) {
          return true;
        }

        return (
          record.key.toLowerCase().includes(query) ||
          record.source.toLowerCase().includes(query) ||
          record.type.toLowerCase().includes(query) ||
          record.payload.toLowerCase().includes(query)
        );
      })
      .sort((a, b) => a.key.localeCompare(b.key));
  }, [records, search, typeFilter]);

  const typeCounts = useMemo(() => {
    const counts = new Map<string, number>();
    for (const record of records) {
      const type = record.type || 'unknown';
      counts.set(type, (counts.get(type) ?? 0) + 1);
    }
    return Array.from(counts.entries()).sort((a, b) => b[1] - a[1]);
  }, [records]);

  const expiringCount = useMemo(
    () => records.filter((record) => record.ttlSeconds >= 0).length,
    [records]
  );

  return (
    <div className="space-y-5">
      <div className="flex flex-wrap items-start justify-between gap-3">
        <div>
          <h1 className="text-2xl font-semibold text-slate-100">Database</h1>
          <p className="mt-1 text-sm text-slate-400">
            Live key snapshot from internal Redis with type, TTL, and value payloads.
          </p>
          <p className="mt-1 text-xs text-slate-500">
            {lastUpdatedMs ? `Last updated ${new Date(lastUpdatedMs).toLocaleTimeString()}` : 'Waiting for first snapshot...'}
          </p>
        </div>

        <button
          type="button"
          onClick={() => setRefreshToken((v) => v + 1)}
          className="rounded-xl border border-slate-700 bg-slate-900 px-3 py-2 text-sm text-slate-200 hover:bg-slate-800"
        >
          Refresh now
        </button>
      </div>

      <div className="grid grid-cols-2 gap-3 md:grid-cols-4">
        <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
          <div className="text-xs text-slate-400">Keys</div>
          <div className="font-mono text-xl text-slate-100">{records.length}</div>
        </div>
        <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
          <div className="text-xs text-slate-400">Expiring Keys</div>
          <div className="font-mono text-xl text-slate-100">{expiringCount}</div>
        </div>
        <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
          <div className="text-xs text-slate-400">Types</div>
          <div className="font-mono text-xl text-slate-100">{typeCounts.length}</div>
        </div>
        <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
          <div className="text-xs text-slate-400">Poll Interval</div>
          <div className="font-mono text-xl text-slate-100">{pollIntervalMs}ms</div>
        </div>
      </div>

      <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
        <div className="flex flex-wrap items-center gap-3">
          <input
            value={search}
            onChange={(e) => setSearch(e.target.value)}
            placeholder="Search keys, types, payload..."
            className="min-w-72 flex-1 rounded-lg border border-slate-700 bg-slate-950 px-3 py-2 text-sm text-slate-100 outline-none placeholder:text-slate-500 focus:border-sky-500"
          />

          <select
            value={typeFilter}
            onChange={(e) => setTypeFilter(e.target.value)}
            className="rounded-lg border border-slate-700 bg-slate-950 px-3 py-2 text-sm text-slate-100 outline-none"
          >
            {knownTypes.map((type) => (
              <option key={type} value={type}>
                {type}
              </option>
            ))}
          </select>

          <label className="flex min-w-60 items-center gap-2 text-xs text-slate-400">
            <input
              type="range"
              min={MIN_POLL_INTERVAL_MS}
              max={MAX_POLL_INTERVAL_MS}
              step={250}
              value={pollIntervalMs}
              onChange={(e) =>
                setPollIntervalMs(
                  Math.max(
                    MIN_POLL_INTERVAL_MS,
                    Math.min(MAX_POLL_INTERVAL_MS, Number(e.target.value))
                  )
                )
              }
            />
            poll: {pollIntervalMs}ms
          </label>
        </div>
      </div>

      {typeCounts.length > 0 && (
        <div className="flex flex-wrap gap-2">
          {typeCounts.map(([type, count]) => (
            <span
              key={type}
              className="rounded-full border border-slate-700 bg-slate-900 px-2 py-1 font-mono text-xs text-slate-300"
            >
              {type}: {count}
            </span>
          ))}
        </div>
      )}

      {errorText && (
        <div className="rounded-xl border border-rose-900 bg-rose-950/40 px-3 py-2 text-sm text-rose-200">
          {errorText}
        </div>
      )}

      <div className="overflow-hidden rounded-2xl border border-slate-800 bg-slate-900/70">
        <div className="grid grid-cols-12 border-b border-slate-800 px-4 py-2 text-xs uppercase tracking-wide text-slate-400">
          <div className="col-span-4">Key</div>
          <div className="col-span-1">Type</div>
          <div className="col-span-1 text-right">Entries</div>
          <div className="col-span-2">TTL</div>
          <div className="col-span-4">Preview</div>
        </div>

        {loading && (
          <div className="px-4 py-6 text-sm text-slate-400">Loading database snapshot...</div>
        )}

        {!loading && filteredRecords.length === 0 && (
          <div className="px-4 py-6 text-sm text-slate-400">No database rows matched this filter.</div>
        )}

        {!loading &&
          filteredRecords.map((record) => {
            const rowId = `${record.source}:${record.key}`;
            const expanded = rowId === expandedRowId;
            return (
              <div key={rowId} className="border-t border-slate-800 first:border-t-0">
                <button
                  type="button"
                  onClick={() => setExpandedRowId(expanded ? null : rowId)}
                  className="grid w-full grid-cols-12 items-center px-4 py-2 text-left text-sm hover:bg-slate-900"
                >
                  <div className="col-span-4">
                    <div className="font-mono text-slate-100">{record.key}</div>
                    <div className="text-xs text-slate-500">{record.source}</div>
                  </div>
                  <div className="col-span-1">
                    <span className="rounded bg-slate-800 px-2 py-1 font-mono text-xs text-slate-200">
                      {record.type}
                    </span>
                  </div>
                  <div className="col-span-1 text-right font-mono text-slate-200">
                    {record.entryCount}
                  </div>
                  <div className="col-span-2 font-mono text-xs text-slate-300">
                    {formatTtl(record.ttlSeconds)}
                  </div>
                  <div className="col-span-4 font-mono text-xs text-slate-300">
                    {payloadPreview(record.payload)}
                  </div>
                </button>

                {expanded && (
                  <div className="border-t border-slate-800 bg-slate-950/70 px-4 py-3">
                    <div className="mb-2 text-xs uppercase tracking-wide text-slate-500">
                      Full payload
                    </div>
                    <pre className="max-h-80 overflow-auto whitespace-pre-wrap break-all rounded-lg border border-slate-800 bg-black/30 p-3 font-mono text-xs text-slate-200">
                      {record.payload || '(empty)'}
                    </pre>
                  </div>
                )}
              </div>
            );
          })}
      </div>
    </div>
  );
}
