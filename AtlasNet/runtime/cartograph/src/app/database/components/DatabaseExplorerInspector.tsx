'use client';

import { useEffect, useMemo, useState } from 'react';
import type { DatabaseRecord } from '../../lib/cartographTypes';
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
const NAMESPACE_DELIMITER = ':';
const DEFAULT_DESKTOP_GRID_COLUMNS_CLASS =
  'lg:grid-cols-[minmax(0,1fr)_minmax(0,1fr)]';

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

interface DatabaseExplorerInspectorProps {
  records: DatabaseRecord[];
  loading?: boolean;
  loadingText?: string;
  emptyText?: string;
  selectionScopeKey?: string | null;
  showControls?: boolean;
  controlsClassName?: string;
  containerClassName?: string;
  explorerViewportClassName?: string;
  inspectorViewportClassName?: string;
  desktopGridColumnsClassName?: string;
  fillHeight?: boolean;
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

function splitNamespaceFolders(key: string): string[] {
  const parts = key.split(NAMESPACE_DELIMITER).filter((p) => p.length > 0);
  if (parts.length <= 1) {
    return [];
  }
  return parts.slice(0, -1);
}

function getRecordIdentity(record: DatabaseRecord): string {
  return `${record.source}\u001f${record.key}`;
}

function folderPartsForRecord(record: DatabaseRecord, groupMode: GroupMode): string[] {
  if (groupMode === 'flat') {
    return [];
  }
  if (groupMode === 'type') {
    return [record.type || 'unknown'];
  }
  if (groupMode === 'type_namespace') {
    return [record.type || 'unknown', ...splitNamespaceFolders(record.key)];
  }
  return splitNamespaceFolders(record.key);
}

function buildExplorerNodes(records: DatabaseRecord[], groupMode: GroupMode): ExplorerNode[] {
  if (groupMode === 'flat') {
    return [...records]
      .sort((a, b) => {
        const keyOrder = a.key.localeCompare(b.key);
        if (keyOrder !== 0) {
          return keyOrder;
        }
        return a.source.localeCompare(b.source);
      })
      .map((record) => ({
        kind: 'key',
        id: getRecordIdentity(record),
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
    const parts = folderPartsForRecord(record, groupMode);
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
      .sort((a, b) => {
        const keyOrder = a.key.localeCompare(b.key);
        if (keyOrder !== 0) {
          return keyOrder;
        }
        return a.source.localeCompare(b.source);
      })
      .map((record) => ({
        kind: 'key',
        id: getRecordIdentity(record),
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
    .sort((a, b) => {
      const keyOrder = a.key.localeCompare(b.key);
      if (keyOrder !== 0) {
        return keyOrder;
      }
      return a.source.localeCompare(b.source);
    })
    .map((record) => ({
      kind: 'key',
      id: getRecordIdentity(record),
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

export function DatabaseExplorerInspector({
  records,
  loading = false,
  loadingText = 'Loading database snapshot...',
  emptyText = 'No keys matched this filter.',
  selectionScopeKey = null,
  showControls = true,
  controlsClassName = '',
  containerClassName = '',
  explorerViewportClassName = 'max-h-[70vh] overflow-auto p-2',
  inspectorViewportClassName = 'max-h-[70vh] overflow-auto p-3',
  desktopGridColumnsClassName = DEFAULT_DESKTOP_GRID_COLUMNS_CLASS,
  fillHeight = false,
}: DatabaseExplorerInspectorProps) {
  const [search, setSearch] = useState('');
  const [typeFilter, setTypeFilter] = useState('all');
  const [groupMode, setGroupMode] = useState<GroupMode>('namespace');
  const [selectedRecordId, setSelectedRecordId] = useState<string | null>(null);
  const [expandedFolderIds, setExpandedFolderIds] = useState<Set<string>>(new Set());
  const [hasInitializedExpansion, setHasInitializedExpansion] = useState(false);

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
      .sort((a, b) => {
        const keyOrder = a.key.localeCompare(b.key);
        if (keyOrder !== 0) {
          return keyOrder;
        }
        return a.source.localeCompare(b.source);
      });
  }, [records, search, typeFilter]);

  const explorerNodes = useMemo(
    () => buildExplorerNodes(filteredRecords, groupMode),
    [filteredRecords, groupMode]
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

  useEffect(() => {
    if (
      selectedRecordId &&
      !records.some((record) => getRecordIdentity(record) === selectedRecordId)
    ) {
      setSelectedRecordId(null);
    }
  }, [records, selectedRecordId]);

  const selectedRecord = useMemo(
    () =>
      records.find((record) => getRecordIdentity(record) === selectedRecordId) ??
      null,
    [records, selectedRecordId]
  );

  useEffect(() => {
    setSelectedRecordId(null);
    setExpandedFolderIds(new Set());
    setHasInitializedExpansion(false);
  }, [selectionScopeKey]);

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

    const selected = selectedRecordId === node.id;
    return (
      <button
        key={node.id}
        type="button"
        onClick={() => setSelectedRecordId(node.id)}
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
    <div
      className={
        (fillHeight ? 'h-full min-h-0 flex flex-col ' : '') + containerClassName
      }
    >
      {showControls && (
        <div
          className={
            'mb-3 rounded-2xl border border-slate-800 bg-slate-900/70 p-3 ' +
            controlsClassName
          }
        >
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
          </div>
        </div>
      )}

      <div
        className={`grid grid-cols-1 gap-4 ${desktopGridColumnsClassName} ${
          fillHeight ? 'min-h-0 flex-1' : ''
        }`}
      >
        <div
          className={
            'overflow-hidden rounded-2xl border border-slate-800 bg-slate-900/70 ' +
            (fillHeight ? 'min-h-0 flex flex-col' : '')
          }
        >
          <div className="border-b border-slate-800 px-3 py-2 text-xs uppercase tracking-wide text-slate-400">
            Explorer
          </div>
          <div
            className={
              (fillHeight ? 'min-h-0 flex-1 ' : '') + explorerViewportClassName
            }
          >
            {loading && (
              <div className="px-2 py-4 text-sm text-slate-400">{loadingText}</div>
            )}

            {!loading && explorerNodes.length === 0 && (
              <div className="px-2 py-4 text-sm text-slate-400">{emptyText}</div>
            )}

            {!loading && explorerNodes.map((node) => renderExplorerNode(node, 0))}
          </div>
        </div>

        <div
          className={
            'overflow-hidden rounded-2xl border border-slate-800 bg-slate-900/70 ' +
            (fillHeight ? 'min-h-0 flex flex-col' : '')
          }
        >
          <div className="border-b border-slate-800 px-3 py-2 text-xs uppercase tracking-wide text-slate-400">
            Inspector
          </div>
          <div
            className={
              (fillHeight ? 'min-h-0 flex-1 ' : '') + inspectorViewportClassName
            }
          >
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
    </div>
  );
}
