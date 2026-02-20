'use client';

import { useEffect, useMemo, useState } from 'react';
import type {
  DatabaseRecord,
  DatabaseSnapshotResponse,
  DatabaseSource,
} from '../lib/cartographTypes';
import {
  ArrowUpDown,
  Braces,
  ChevronDown,
  ChevronRight,
  CircleHelp,
  CircleOff,
  Copy,
  FileJson,
  Folder,
  Hash,
  KeyRound,
  List,
} from 'lucide-react';
import { HardcodedDecodeToggle } from './readOnlyDecode/HardcodedDecodeToggle';

const DEFAULT_POLL_INTERVAL_MS = 1000;
const MIN_POLL_INTERVAL_MS = 250;
const MAX_POLL_INTERVAL_MS = 5000;
const POLL_DISABLED_AT_MS = MAX_POLL_INTERVAL_MS;
const REDIS_TYPE_OPTIONS = [
  'all',
  'string',
  'list',
  'set',
  'zset',
  'hash',
  'stream',
  'vectorset',
  'module',
  'json',
  'ReJSON-RL',
  'none',
  'unknown',
];
const GROUP_MODE_OPTIONS = [
  { value: 'namespace', label: 'Namespace' },
  { value: 'type', label: 'Type' },
  { value: 'type_namespace', label: 'Type + Namespace' },
  { value: 'flat', label: 'Flat' },
] as const;
const DELIMITER_OPTIONS = [':', '/', '.', '|'] as const;

type GroupMode = (typeof GROUP_MODE_OPTIONS)[number]['value'];

type ExplorerNode = ExplorerFolderNode | ExplorerKeyNode;

interface ExplorerFolderNode {
  kind: 'folder';
  id: string;
  name: string;
  count: number;
  children: ExplorerNode[];
}

interface ExplorerKeyNode {
  kind: 'key';
  id: string;
  record: DatabaseRecord;
}

interface MutableFolder {
  id: string;
  name: string;
  folders: Map<string, MutableFolder>;
  keys: DatabaseRecord[];
}

function formatTtl(ttlSeconds: number): string {
  if (ttlSeconds === -1) return 'persistent';
  if (ttlSeconds === -2) return 'missing';
  return `${ttlSeconds}s`;
}

function payloadPreview(payload: string): string {
  const normalized = payload.replace(/\s+/g, ' ').trim();
  if (normalized.length === 0) return '(empty)';
  if (normalized.length <= 96) return normalized;
  return `${normalized.slice(0, 96)}...`;
}

function formatPayload(record: DatabaseRecord): string {
  const rawPayload = record.payload || '(empty)';
  const trimmedPayload = (record.payload || '').trim();

  const shouldParseAsJson =
    record.type === 'json' ||
    record.type === 'ReJSON-RL' ||
    (record.type === 'string' &&
      (trimmedPayload.startsWith('{') || trimmedPayload.startsWith('[')));

  if (!shouldParseAsJson) return rawPayload;

  try {
    return JSON.stringify(JSON.parse(record.payload), null, 2);
  } catch {
    return rawPayload;
  }
}

function splitNamespaceFolders(key: string, delimiter: string): string[] {
  const parts = key.split(delimiter).filter((p) => p.length > 0);
  if (parts.length <= 1) {
    return [];
  }
  return parts.slice(0, -1);
}

function folderPartsForRecord(
  record: DatabaseRecord,
  groupMode: GroupMode,
  delimiter: string
): string[] {
  if (groupMode === 'flat') {
    return [];
  }
  if (groupMode === 'type') {
    return [record.type || 'unknown'];
  }
  if (groupMode === 'type_namespace') {
    return [record.type || 'unknown', ...splitNamespaceFolders(record.key, delimiter)];
  }
  return splitNamespaceFolders(record.key, delimiter);
}

function buildExplorerNodes(
  records: DatabaseRecord[],
  groupMode: GroupMode,
  delimiter: string
): ExplorerNode[] {
  if (groupMode === 'flat') {
    return [...records]
      .sort((a, b) => a.key.localeCompare(b.key))
      .map((record) => ({
        kind: 'key',
        id: record.key,
        record,
      }));
  }

  const root: MutableFolder = {
    id: 'root',
    name: 'root',
    folders: new Map(),
    keys: [],
  };

  for (const record of records) {
    const parts = folderPartsForRecord(record, groupMode, delimiter);
    let current = root;
    let folderPath = root.id;

    for (const part of parts) {
      folderPath = `${folderPath}\u001f${part}`;
      let child = current.folders.get(part);
      if (!child) {
        child = {
          id: `folder:${folderPath}`,
          name: part,
          folders: new Map(),
          keys: [],
        };
        current.folders.set(part, child);
      }
      current = child;
    }

    current.keys.push(record);
  }

  const finalizeFolder = (folder: MutableFolder): ExplorerFolderNode => {
    const folderChildren = Array.from(folder.folders.values())
      .sort((a, b) => a.name.localeCompare(b.name))
      .map(finalizeFolder);
    const keyChildren: ExplorerKeyNode[] = [...folder.keys]
      .sort((a, b) => a.key.localeCompare(b.key))
      .map((record) => ({
        kind: 'key',
        id: record.key,
        record,
      }));

    return {
      kind: 'folder',
      id: folder.id,
      name: folder.name,
      count:
        keyChildren.length + folderChildren.reduce((sum, node) => sum + node.count, 0),
      children: [...folderChildren, ...keyChildren],
    };
  };

  const topFolders = Array.from(root.folders.values())
    .sort((a, b) => a.name.localeCompare(b.name))
    .map(finalizeFolder);
  const topKeys: ExplorerKeyNode[] = [...root.keys]
    .sort((a, b) => a.key.localeCompare(b.key))
    .map((record) => ({
      kind: 'key',
      id: record.key,
      record,
    }));

  return [...topFolders, ...topKeys];
}

function TypeIcon({ type }: { type: string }) {
  switch (type) {
    case 'string':
      return <KeyRound className="h-3.5 w-3.5 text-amber-300" />;
    case 'hash':
      return <Braces className="h-3.5 w-3.5 text-sky-300" />;
    case 'set':
      return <Hash className="h-3.5 w-3.5 text-emerald-300" />;
    case 'zset':
      return <ArrowUpDown className="h-3.5 w-3.5 text-cyan-300" />;
    case 'list':
      return <List className="h-3.5 w-3.5 text-violet-300" />;
    case 'json':
    case 'ReJSON-RL':
      return <FileJson className="h-3.5 w-3.5 text-orange-300" />;
    case 'none':
      return <CircleOff className="h-3.5 w-3.5 text-slate-500" />;
    default:
      return <CircleHelp className="h-3.5 w-3.5 text-slate-400" />;
  }
}

export default function DatabasePage() {
  const [sources, setSources] = useState<DatabaseSource[]>([]);
  const [selectedSource, setSelectedSource] = useState<string | null>(null);
  const [records, setRecords] = useState<DatabaseRecord[]>([]);
  const [search, setSearch] = useState('');
  const [typeFilter, setTypeFilter] = useState('all');
  const [groupMode, setGroupMode] = useState<GroupMode>('namespace');
  const [namespaceDelimiter, setNamespaceDelimiter] = useState(':');
  const [decodeSerialized, setDecodeSerialized] = useState(true);
  const [selectedKey, setSelectedKey] = useState<string | null>(null);
  const [expandedFolderIds, setExpandedFolderIds] = useState<Set<string>>(new Set());
  const [hasInitializedExpansion, setHasInitializedExpansion] = useState(false);
  const [pollIntervalMs, setPollIntervalMs] = useState(DEFAULT_POLL_INTERVAL_MS);
  const [refreshToken, setRefreshToken] = useState(0);
  const [loading, setLoading] = useState(true);
  const [errorText, setErrorText] = useState<string | null>(null);
  const [lastUpdatedMs, setLastUpdatedMs] = useState<number | null>(null);

  useEffect(() => {
    let alive = true;

    async function poll() {
      try {
        const params = new URLSearchParams();
        if (selectedSource) {
          params.set('source', selectedSource);
        }
        params.set('decodeSerialized', decodeSerialized ? '1' : '0');
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

    poll();
    const intervalId =
      pollIntervalMs >= POLL_DISABLED_AT_MS
        ? null
        : setInterval(poll, pollIntervalMs);
    return () => {
      alive = false;
      if (intervalId != null) {
        clearInterval(intervalId);
      }
    };
  }, [decodeSerialized, pollIntervalMs, refreshToken, selectedSource]);

  useEffect(() => {
    if (selectedKey && !records.some((record) => record.key === selectedKey)) {
      setSelectedKey(null);
    }
  }, [records, selectedKey]);

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
          record.type.toLowerCase().includes(query) ||
          record.payload.toLowerCase().includes(query)
        );
      })
      .sort((a, b) => a.key.localeCompare(b.key));
  }, [records, search, typeFilter]);

  const explorerNodes = useMemo(
    () => buildExplorerNodes(filteredRecords, groupMode, namespaceDelimiter),
    [filteredRecords, groupMode, namespaceDelimiter]
  );

  useEffect(() => {
    if (hasInitializedExpansion) {
      return;
    }
    const topLevelFolders = explorerNodes
      .filter((node): node is ExplorerFolderNode => node.kind === 'folder')
      .map((node) => node.id);
    if (topLevelFolders.length === 0) {
      return;
    }
    setExpandedFolderIds(new Set(topLevelFolders));
    setHasInitializedExpansion(true);
  }, [explorerNodes, hasInitializedExpansion]);

  const selectedRecord = useMemo(
    () => records.find((record) => record.key === selectedKey) ?? null,
    [records, selectedKey]
  );

  const selectedSourceView =
    sources.find((source) => source.id === selectedSource) ?? null;

  function toggleFolder(folderId: string) {
    setExpandedFolderIds((prev) => {
      const next = new Set(prev);
      if (next.has(folderId)) {
        next.delete(folderId);
      } else {
        next.add(folderId);
      }
      return next;
    });
  }

  function copyToClipboard(value: string) {
    if (!navigator?.clipboard) {
      return;
    }
    void navigator.clipboard.writeText(value);
  }

  function renderExplorerNode(node: ExplorerNode, depth: number): JSX.Element {
    if (node.kind === 'folder') {
      const expanded = expandedFolderIds.has(node.id);
      return (
        <div key={node.id}>
          <button
            type="button"
            onClick={() => toggleFolder(node.id)}
            className="flex w-full items-center gap-1 rounded px-2 py-1 text-left text-sm hover:bg-slate-900"
            style={{ paddingLeft: `${depth * 14 + 8}px` }}
          >
            {expanded ? (
              <ChevronDown className="h-3.5 w-3.5 text-slate-400" />
            ) : (
              <ChevronRight className="h-3.5 w-3.5 text-slate-400" />
            )}
            <Folder className="h-3.5 w-3.5 text-amber-300" />
            <span className="truncate text-slate-200">{node.name}</span>
            <span className="ml-auto font-mono text-xs text-slate-500">{node.count}</span>
          </button>
          {expanded &&
            node.children.map((child) => renderExplorerNode(child, depth + 1))}
        </div>
      );
    }

    const selected = selectedKey === node.record.key;
    return (
      <button
        key={node.id}
        type="button"
        onClick={() => setSelectedKey(node.record.key)}
        className={
          'flex w-full items-center gap-2 rounded px-2 py-1 text-left text-sm hover:bg-slate-900 ' +
          (selected ? 'bg-sky-900/30 ring-1 ring-sky-700/70' : '')
        }
        style={{ paddingLeft: `${depth * 14 + 22}px` }}
      >
        <TypeIcon type={node.record.type} />
        <span className="truncate font-mono text-slate-100">{node.record.key}</span>
        <span className="ml-auto font-mono text-xs text-slate-500">{node.record.type}</span>
      </button>
    );
  }

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
              onClick={() => {
                setSelectedSource(source.id);
                setSelectedKey(null);
                setExpandedFolderIds(new Set());
                setHasInitializedExpansion(false);
              }}
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
            onClick={() => setRefreshToken((v) => v + 1)}
            className="rounded-xl border border-slate-700 bg-slate-900 px-3 py-2 text-sm text-slate-200 hover:bg-slate-800"
          >
            Refresh now
          </button>
        </div>

        <div className="grid grid-cols-2 gap-3 md:grid-cols-3">
          <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
            <div className="text-xs text-slate-400">Filtered Keys</div>
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
              {REDIS_TYPE_OPTIONS.map((type) => (
                <option key={type} value={type}>
                  {type}
                </option>
              ))}
            </select>
            <select
              value={groupMode}
              onChange={(e) => {
                setGroupMode(e.target.value as GroupMode);
                setExpandedFolderIds(new Set());
                setHasInitializedExpansion(false);
              }}
              className="rounded-lg border border-slate-700 bg-slate-950 px-3 py-2 text-sm text-slate-100 outline-none"
            >
              {GROUP_MODE_OPTIONS.map((option) => (
                <option key={option.value} value={option.value}>
                  group: {option.label}
                </option>
              ))}
            </select>
            <select
              value={namespaceDelimiter}
              disabled={groupMode === 'flat' || groupMode === 'type'}
              onChange={(e) => {
                setNamespaceDelimiter(e.target.value);
                setExpandedFolderIds(new Set());
                setHasInitializedExpansion(false);
              }}
              className="rounded-lg border border-slate-700 bg-slate-950 px-3 py-2 text-sm text-slate-100 outline-none disabled:opacity-50"
            >
              {DELIMITER_OPTIONS.map((delimiter) => (
                <option key={delimiter} value={delimiter}>
                  delim: {delimiter}
                </option>
              ))}
            </select>
            <HardcodedDecodeToggle
              enabled={decodeSerialized}
              onChange={setDecodeSerialized}
            />
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
              poll interval:{' '}
              {pollIntervalMs >= POLL_DISABLED_AT_MS
                ? 'off'
                : `${pollIntervalMs}ms`}
            </label>
          </div>
        </div>

        {errorText && (
          <div className="rounded-xl border border-rose-900 bg-rose-950/40 px-3 py-2 text-sm text-rose-200">
            {errorText}
          </div>
        )}

        <div className="grid grid-cols-1 gap-4 lg:grid-cols-[minmax(0,1fr)_minmax(0,1fr)]">
          <div className="overflow-hidden rounded-2xl border border-slate-800 bg-slate-900/70">
            <div className="border-b border-slate-800 px-3 py-2 text-xs uppercase tracking-wide text-slate-400">
              Explorer
            </div>
            <div className="max-h-[70vh] overflow-auto p-2">
              {loading && (
                <div className="px-2 py-4 text-sm text-slate-400">
                  Loading database snapshot...
                </div>
              )}

              {!loading && explorerNodes.length === 0 && (
                <div className="px-2 py-4 text-sm text-slate-400">
                  No keys matched this filter.
                </div>
              )}

              {!loading &&
                explorerNodes.map((node) => renderExplorerNode(node, 0))}
            </div>
          </div>

          <div className="overflow-hidden rounded-2xl border border-slate-800 bg-slate-900/70">
            <div className="border-b border-slate-800 px-3 py-2 text-xs uppercase tracking-wide text-slate-400">
              Inspector
            </div>
            <div className="max-h-[70vh] overflow-auto p-3">
              {!selectedRecord && (
                <div className="py-6 text-sm text-slate-400">
                  Select a key from the explorer to inspect its value.
                </div>
              )}

              {selectedRecord && (
                <div className="space-y-3">
                  <div className="flex items-center gap-2">
                    <TypeIcon type={selectedRecord.type} />
                    <span className="truncate font-mono text-sm text-slate-100">
                      {selectedRecord.key}
                    </span>
                  </div>

                  <div className="flex flex-wrap gap-2">
                    <span className="rounded border border-slate-700 bg-slate-900 px-2 py-1 font-mono text-xs text-slate-200">
                      type: {selectedRecord.type}
                    </span>
                    <span className="rounded border border-slate-700 bg-slate-900 px-2 py-1 font-mono text-xs text-slate-200">
                      entries: {selectedRecord.entryCount}
                    </span>
                    <span className="rounded border border-slate-700 bg-slate-900 px-2 py-1 font-mono text-xs text-slate-200">
                      ttl: {formatTtl(selectedRecord.ttlSeconds)}
                    </span>
                    <span className="rounded border border-slate-700 bg-slate-900 px-2 py-1 font-mono text-xs text-slate-200">
                      preview: {payloadPreview(selectedRecord.payload)}
                    </span>
                  </div>

                  <div className="flex gap-2">
                    <button
                      type="button"
                      onClick={() => copyToClipboard(selectedRecord.key)}
                      className="inline-flex items-center gap-1 rounded border border-slate-700 bg-slate-900 px-2 py-1 text-xs text-slate-200 hover:bg-slate-800"
                    >
                      <Copy className="h-3.5 w-3.5" />
                      key
                    </button>
                    <button
                      type="button"
                      onClick={() => copyToClipboard(formatPayload(selectedRecord))}
                      className="inline-flex items-center gap-1 rounded border border-slate-700 bg-slate-900 px-2 py-1 text-xs text-slate-200 hover:bg-slate-800"
                    >
                      <Copy className="h-3.5 w-3.5" />
                      payload
                    </button>
                  </div>

                  <pre className="max-h-[52vh] overflow-auto whitespace-pre-wrap break-all rounded-lg border border-slate-800 bg-black/30 p-3 font-mono text-xs text-slate-200">
                    {formatPayload(selectedRecord)}
                  </pre>
                </div>
              )}
            </div>
          </div>
        </div>
      </section>
    </div>
  );
}
