'use client';

import { Fragment, useEffect, useMemo, useRef, useState } from 'react';
import type {
  AuthorityEntityTelemetry,
  DatabaseRecord,
  DatabaseSnapshotResponse,
} from '../lib/cartographTypes';
import { useAuthorityEntities } from '../lib/hooks/useTelemetryFeeds';
import { DatabaseExplorerInspector } from '../database/components/DatabaseExplorerInspector';
import { useEntityDatabaseDetails } from '../map/entityInspector/useEntityDatabaseDetails';
import { HardcodedDecodeToggle } from '../database/RevertByteOptimation/HardcodedDecodeToggle';

const DEFAULT_AUTHORITY_POLL_INTERVAL_MS = 250;
const MIN_AUTHORITY_POLL_INTERVAL_MS = 50;
const MAX_AUTHORITY_POLL_INTERVAL_MS = 1000;
const DEFAULT_DATABASE_POLL_INTERVAL_MS = 1000;
const MIN_DATABASE_POLL_INTERVAL_MS = 250;
const MAX_DATABASE_POLL_INTERVAL_MS = 5000;
const DATABASE_POLL_DISABLED_AT_MS = MAX_DATABASE_POLL_INTERVAL_MS;
const ENTITY_DETAIL_POLL_INTERVAL_MS = 0;

const AUTHORITY_ENTITY_SNAPSHOTS_KEY = 'Authority_EntitySnapshots';

interface ClientGroup {
  key: string;
  clientId: string;
  entities: AuthorityEntityTelemetry[];
  ownerIds: string[];
  worlds: number[];
}

function formatPosition(value: number): string {
  if (!Number.isFinite(value)) {
    return 'n/a';
  }
  if (Math.abs(value) >= 1000 || (Math.abs(value) > 0 && Math.abs(value) < 0.001)) {
    return value.toExponential(2);
  }
  return value.toFixed(2).replace(/\.?0+$/, '');
}

function formatUpdatedAt(updatedAtMs: number | null): string {
  if (updatedAtMs == null) {
    return 'never';
  }
  return new Date(updatedAtMs).toLocaleTimeString();
}

function entityRowKey(entity: AuthorityEntityTelemetry): string {
  return `${entity.entityId}:${entity.ownerId}:${entity.world}`;
}

function parseEntitySnapshotLine(line: string): AuthorityEntityTelemetry | null {
  const separator = line.indexOf('\t');
  const body = separator >= 0 ? line.slice(separator + 1) : line;

  const owner = /owner=([^\s]+)/.exec(body)?.[1] ?? '';
  const entityId = /entity=([^\s]+)/.exec(body)?.[1] ?? '';
  const worldRaw = /world=([^\s]+)/.exec(body)?.[1] ?? '';
  const pos = /pos=\(([^,]+),([^,]+),([^)]+)\)/.exec(body);
  const isClientRaw = /isClient=([^\s]+)/.exec(body)?.[1] ?? '0';
  const clientId = /clientId=([^\s]+)/.exec(body)?.[1] ?? '';

  if (!entityId || !owner || !worldRaw || !pos) {
    return null;
  }

  const world = Number(worldRaw);
  const x = Number(pos[1]);
  const y = Number(pos[2]);
  const z = Number(pos[3]);
  if (
    !Number.isFinite(world) ||
    !Number.isFinite(x) ||
    !Number.isFinite(y) ||
    !Number.isFinite(z)
  ) {
    return null;
  }

  const normalizedIsClient = isClientRaw.trim().toLowerCase();
  const isClient =
    normalizedIsClient === '1' ||
    normalizedIsClient === 'true' ||
    normalizedIsClient === 'yes' ||
    normalizedIsClient === 'on';

  return {
    entityId,
    ownerId: owner,
    world,
    x,
    y,
    z,
    isClient,
    clientId,
  };
}

function parseEntitiesFromDatabaseRecords(records: DatabaseRecord[]): AuthorityEntityTelemetry[] {
  const out: AuthorityEntityTelemetry[] = [];
  for (const record of records) {
    if (record.key !== AUTHORITY_ENTITY_SNAPSHOTS_KEY) {
      continue;
    }
    const lines = record.payload.split(/\r?\n/);
    for (const line of lines) {
      const parsed = parseEntitySnapshotLine(line);
      if (parsed) {
        out.push(parsed);
      }
    }
  }
  return out;
}

function mergeEntities(
  authorityRows: AuthorityEntityTelemetry[],
  databaseRows: AuthorityEntityTelemetry[]
): AuthorityEntityTelemetry[] {
  const byKey = new Map<string, AuthorityEntityTelemetry>();
  for (const row of databaseRows) {
    byKey.set(entityRowKey(row), row);
  }
  for (const row of authorityRows) {
    byKey.set(entityRowKey(row), row);
  }
  return Array.from(byKey.values()).sort((left, right) =>
    entityRowKey(left).localeCompare(entityRowKey(right))
  );
}

function buildClientGroups(
  entities: AuthorityEntityTelemetry[],
  includeNonClientEntities: boolean
): ClientGroup[] {
  const groupedByClientId = new Map<string, AuthorityEntityTelemetry[]>();
  const clientRows = entities.filter((entity) => entity.isClient);

  for (const entity of clientRows) {
    const clientId = entity.clientId.trim();
    const key = clientId.length > 0 ? clientId : `entity:${entity.entityId}`;
    let bucket = groupedByClientId.get(key);
    if (!bucket) {
      bucket = [];
      groupedByClientId.set(key, bucket);
    }
    bucket.push(entity);
  }

  if (includeNonClientEntities) {
    for (const entity of entities) {
      const clientId = entity.clientId.trim();
      if (!clientId) {
        continue;
      }
      let bucket = groupedByClientId.get(clientId);
      if (!bucket) {
        bucket = [];
        groupedByClientId.set(clientId, bucket);
      }
      if (!bucket.some((row) => entityRowKey(row) === entityRowKey(entity))) {
        bucket.push(entity);
      }
    }
  }

  const out: ClientGroup[] = [];
  for (const [key, rows] of groupedByClientId.entries()) {
    const sorted = [...rows].sort((left, right) =>
      left.entityId.localeCompare(right.entityId)
    );
    const ownerIds = Array.from(
      new Set(sorted.map((entity) => entity.ownerId).filter((id) => id.length > 0))
    );
    const worlds = Array.from(new Set(sorted.map((entity) => entity.world))).sort(
      (left, right) => left - right
    );
    out.push({
      key,
      clientId: sorted[0]?.clientId ?? '',
      entities: sorted,
      ownerIds,
      worlds,
    });
  }

  out.sort((left, right) => left.key.localeCompare(right.key));
  return out;
}

export default function ClientsPage() {
  const [authorityPollIntervalMs, setAuthorityPollIntervalMs] = useState(
    DEFAULT_AUTHORITY_POLL_INTERVAL_MS
  );
  const [includeNonClientEntities, setIncludeNonClientEntities] = useState(false);
  const [selectedClientKey, setSelectedClientKey] = useState<string | null>(null);
  const [activeEntityId, setActiveEntityId] = useState<string | null>(null);
  const [entitySearch, setEntitySearch] = useState('');
  const [decodeSerialized, setDecodeSerialized] = useState(true);
  const [databasePollIntervalMs, setDatabasePollIntervalMs] = useState(
    DEFAULT_DATABASE_POLL_INTERVAL_MS
  );
  const [authorityUpdatedMs, setAuthorityUpdatedMs] = useState<number | null>(null);
  const [databaseUpdatedMs, setDatabaseUpdatedMs] = useState<number | null>(null);
  const [databaseRows, setDatabaseRows] = useState<AuthorityEntityTelemetry[]>([]);
  const [databaseErrorText, setDatabaseErrorText] = useState<string | null>(null);
  const decodeSerializedRef = useRef(decodeSerialized);

  const authorityRows = useAuthorityEntities({
    intervalMs: authorityPollIntervalMs,
    enabled: true,
    resetOnException: false,
    resetOnHttpError: false,
  });

  useEffect(() => {
    setAuthorityUpdatedMs(Date.now());
  }, [authorityRows]);

  useEffect(() => {
    decodeSerializedRef.current = decodeSerialized;
  }, [decodeSerialized]);

  useEffect(() => {
    let alive = true;

    async function pollDatabaseSnapshot() {
      try {
        const response = await fetch(
          `/api/databases?decodeSerialized=${decodeSerializedRef.current ? '1' : '0'}`,
          {
          cache: 'no-store',
          }
        );
        if (!response.ok) {
          if (!alive) {
            return;
          }
          setDatabaseErrorText(`DB snapshot failed (${response.status}).`);
          return;
        }

        const payload = (await response.json()) as DatabaseSnapshotResponse;
        if (!alive) {
          return;
        }
        const records = Array.isArray(payload.records) ? payload.records : [];
        setDatabaseRows(parseEntitiesFromDatabaseRecords(records));
        setDatabaseUpdatedMs(Date.now());
        setDatabaseErrorText(null);
      } catch {
        if (!alive) {
          return;
        }
        setDatabaseErrorText('Failed to fetch DB snapshot.');
      }
    }

    void pollDatabaseSnapshot();
    const intervalId =
      databasePollIntervalMs >= DATABASE_POLL_DISABLED_AT_MS
        ? null
        : setInterval(() => {
            void pollDatabaseSnapshot();
          }, databasePollIntervalMs);

    return () => {
      alive = false;
      if (intervalId != null) {
        clearInterval(intervalId);
      }
    };
  }, [databasePollIntervalMs]);

  const entities = useMemo(
    () => mergeEntities(authorityRows, databaseRows),
    [authorityRows, databaseRows]
  );
  const visibleEntities = useMemo(
    () =>
      includeNonClientEntities ? entities : entities.filter((entity) => entity.isClient),
    [entities, includeNonClientEntities]
  );
  const clients = useMemo(
    () => buildClientGroups(visibleEntities, includeNonClientEntities),
    [visibleEntities, includeNonClientEntities]
  );

  useEffect(() => {
    if (clients.length === 0) {
      setSelectedClientKey(null);
      return;
    }
    if (!selectedClientKey || !clients.some((client) => client.key === selectedClientKey)) {
      setSelectedClientKey(clients[0].key);
    }
  }, [clients, selectedClientKey]);

  const selectedClient = useMemo(
    () =>
      selectedClientKey
        ? clients.find((client) => client.key === selectedClientKey) ?? null
        : null,
    [clients, selectedClientKey]
  );

  const entitiesInScope = useMemo(() => {
    if (selectedClient) {
      return selectedClient.entities;
    }
    return visibleEntities;
  }, [selectedClient, visibleEntities]);

  const filteredEntities = useMemo(() => {
    const query = entitySearch.trim().toLowerCase();
    if (!query) {
      return entitiesInScope;
    }
    return entitiesInScope.filter((entity) => {
      if (entity.entityId.toLowerCase().includes(query)) {
        return true;
      }
      if (entity.ownerId.toLowerCase().includes(query)) {
        return true;
      }
      if (entity.clientId.toLowerCase().includes(query)) {
        return true;
      }
      return String(entity.world).includes(query);
    });
  }, [entitiesInScope, entitySearch]);

  useEffect(() => {
    if (
      activeEntityId &&
      !entities.some((entity) => entity.entityId === activeEntityId)
    ) {
      setActiveEntityId(null);
    }
  }, [activeEntityId, entities]);

  const activeEntity = useMemo(
    () => entities.find((entity) => entity.entityId === activeEntityId) ?? null,
    [activeEntityId, entities]
  );
  const details = useEntityDatabaseDetails(activeEntity, ENTITY_DETAIL_POLL_INTERVAL_MS);

  const totalClients = entities.filter((entity) => entity.isClient).length;
  const totalEntities = entities.length;
  const clientsOnline = 0;

  return (
    <div className="space-y-5">
      <header className="space-y-1">
        <h1 className="text-2xl font-semibold text-slate-100">Clients</h1>
        <p className="text-sm text-slate-400">
          Data source: authority telemetry with DB fallback from `Authority_EntitySnapshots`.
        </p>
      </header>

      <section className="grid gap-3 md:grid-cols-3">
        <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
          <div className="text-xs text-slate-400">Total Clients</div>
          <div className="font-mono text-xl text-slate-100">{totalClients}</div>
        </div>
        <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
          <div className="text-xs text-slate-400">Total Entities</div>
          <div className="font-mono text-xl text-slate-100">{totalEntities}</div>
        </div>
        <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
          <div className="text-xs text-slate-400">Clients Online</div>
          <div className="font-mono text-xl text-slate-100">{clientsOnline}</div>
        </div>
      </section>

      <section className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
        <div className="flex flex-wrap items-center gap-3 text-xs text-slate-400">
          <label className="flex min-w-64 items-center gap-2">
            authority poll:
            <input
              type="range"
              min={MIN_AUTHORITY_POLL_INTERVAL_MS}
              max={MAX_AUTHORITY_POLL_INTERVAL_MS}
              step={50}
              value={authorityPollIntervalMs}
              onChange={(event) =>
                setAuthorityPollIntervalMs(
                  Math.max(
                    MIN_AUTHORITY_POLL_INTERVAL_MS,
                    Math.min(MAX_AUTHORITY_POLL_INTERVAL_MS, Number(event.target.value))
                  )
                )
              }
            />
            {authorityPollIntervalMs}ms
          </label>

          <label className="flex items-center gap-2">
            <input
              type="checkbox"
              checked={includeNonClientEntities}
              onChange={(event) => setIncludeNonClientEntities(event.target.checked)}
            />
            include non-isClient entities
          </label>

          <HardcodedDecodeToggle enabled={decodeSerialized} onChange={setDecodeSerialized} />

          <label className="flex min-w-64 items-center gap-2">
            db poll:{' '}
            {databasePollIntervalMs >= DATABASE_POLL_DISABLED_AT_MS
              ? 'off'
              : `${databasePollIntervalMs}ms`}
            <input
              type="range"
              min={MIN_DATABASE_POLL_INTERVAL_MS}
              max={MAX_DATABASE_POLL_INTERVAL_MS}
              step={250}
              value={databasePollIntervalMs}
              onChange={(event) =>
                setDatabasePollIntervalMs(
                  Math.max(
                    MIN_DATABASE_POLL_INTERVAL_MS,
                    Math.min(MAX_DATABASE_POLL_INTERVAL_MS, Number(event.target.value))
                  )
                )
              }
            />
          </label>

          <div>authority updated: {formatUpdatedAt(authorityUpdatedMs)}</div>
          <div>db updated: {formatUpdatedAt(databaseUpdatedMs)}</div>
        </div>
        {databaseErrorText && (
          <div className="mt-2 rounded border border-rose-900 bg-rose-950/40 px-3 py-2 text-xs text-rose-200">
            {databaseErrorText}
          </div>
        )}
      </section>

      <section className="grid grid-cols-1 gap-4 xl:grid-cols-[360px_minmax(0,1fr)]">
        <aside className="rounded-2xl border border-slate-800 bg-slate-900/70">
          <div className="border-b border-slate-800 px-3 py-2 text-xs uppercase tracking-wide text-slate-400">
            Client Index ({clients.length})
          </div>
          <div className="max-h-[70vh] overflow-auto divide-y divide-slate-800">
            <button
              type="button"
              onClick={() => setSelectedClientKey(null)}
              className={
                'w-full px-3 py-3 text-left text-xs ' +
                (selectedClientKey == null
                  ? 'bg-sky-900/30 text-sky-100'
                  : 'text-slate-200 hover:bg-slate-900')
              }
            >
              All Entities ({visibleEntities.length})
            </button>
            {clients.map((client) => {
              const active = client.key === selectedClientKey;
              const label = client.clientId || `(missing clientId) ${client.key}`;
              return (
                <button
                  key={client.key}
                  type="button"
                  onClick={() => setSelectedClientKey(client.key)}
                  className={
                    'w-full px-3 py-3 text-left ' +
                    (active
                      ? 'bg-sky-900/30 text-sky-100'
                      : 'text-slate-200 hover:bg-slate-900')
                  }
                >
                  <div className="truncate font-mono text-xs">{label}</div>
                  <div className="mt-1 text-xs text-slate-400">
                    entities: {client.entities.length} | owners: {client.ownerIds.length}
                  </div>
                </button>
              );
            })}
          </div>
        </aside>

        <div className="space-y-3 rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
          <div className="text-xs text-slate-400">
            {selectedClient
              ? `Client scope: ${selectedClient.clientId || selectedClient.key}`
              : 'Client scope: all entities'}
          </div>

          <input
            type="search"
            value={entitySearch}
            onChange={(event) => setEntitySearch(event.target.value)}
            placeholder="Filter entities by entity id / owner / client / world"
            className="w-full rounded border border-slate-700 bg-slate-950 px-3 py-2 text-xs text-slate-100 placeholder:text-slate-500"
          />

          <div className="overflow-x-auto rounded-xl border border-slate-800">
            <table className="w-full min-w-[760px] text-left text-xs">
              <thead className="bg-slate-950 text-slate-400">
                <tr>
                  <th className="px-3 py-2 font-medium">Entity</th>
                  <th className="px-3 py-2 font-medium">Owner</th>
                  <th className="px-3 py-2 font-medium">World</th>
                  <th className="px-3 py-2 font-medium">Position</th>
                  <th className="px-3 py-2 font-medium">isClient</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-slate-800">
                {filteredEntities.length === 0 && (
                  <tr>
                    <td colSpan={5} className="px-3 py-4 text-center text-slate-500">
                      No entities matched this filter.
                    </td>
                  </tr>
                )}

                {filteredEntities.map((entity) => {
                  const rowKey = entityRowKey(entity);
                  const expanded = activeEntityId === entity.entityId;

                  return (
                    <Fragment key={rowKey}>
                      <tr
                        onClick={() =>
                          setActiveEntityId((previous) =>
                            previous === entity.entityId ? null : entity.entityId
                          )
                        }
                        className={
                          'cursor-pointer text-slate-200 hover:bg-slate-900/70 ' +
                          (expanded ? 'bg-sky-950/30' : '')
                        }
                      >
                        <td className="px-3 py-2 font-mono">{entity.entityId}</td>
                        <td className="px-3 py-2 font-mono">{entity.ownerId}</td>
                        <td className="px-3 py-2 font-mono">{entity.world}</td>
                        <td className="px-3 py-2 font-mono">
                          ({formatPosition(entity.x)}, {formatPosition(entity.y)},{' '}
                          {formatPosition(entity.z)})
                        </td>
                        <td className="px-3 py-2">
                          {entity.isClient ? (
                            <span className="rounded bg-emerald-950/60 px-2 py-1 text-emerald-300">
                              true
                            </span>
                          ) : (
                            <span className="rounded bg-slate-800 px-2 py-1 text-slate-300">
                              false
                            </span>
                          )}
                        </td>
                      </tr>

                      {expanded && (
                        <tr className="bg-slate-950/40">
                          <td colSpan={5} className="px-3 py-3">
                            <div className="space-y-3">
                              <div className="text-xs text-slate-400">
                                Click this row again to collapse details.
                              </div>

                              {details.loading && !details.data && (
                                <div className="text-xs text-slate-400">
                                  Loading database details...
                                </div>
                              )}

                              {details.error && !details.data && (
                                <div className="rounded border border-rose-900 bg-rose-950/40 px-3 py-2 text-xs text-rose-200">
                                  {details.error}
                                </div>
                              )}

                              {details.data && (
                                <>
                                  <div className="flex flex-wrap gap-3 text-xs text-slate-400">
                                    <span>matches: {details.data.totalMatches}</span>
                                    <span>sources: {details.data.sourceSummaries.length}</span>
                                    {details.data.truncated && <span>truncated</span>}
                                  </div>
                                  <div className="text-xs text-slate-500">
                                    search terms: {details.data.terms.join(', ')}
                                  </div>

                                  <DatabaseExplorerInspector
                                    records={details.data.records}
                                    loading={false}
                                    selectionScopeKey={entity.entityId}
                                    showControls
                                    explorerViewportClassName="max-h-[30vh] overflow-auto p-2"
                                    inspectorViewportClassName="max-h-[30vh] overflow-auto p-3"
                                  />
                                </>
                              )}

                              {details.error && details.data && (
                                <div className="rounded border border-rose-900 bg-rose-950/40 px-3 py-2 text-xs text-rose-200">
                                  {details.error}
                                </div>
                              )}
                            </div>
                          </td>
                        </tr>
                      )}
                    </Fragment>
                  );
                })}
              </tbody>
            </table>
          </div>
        </div>
      </section>
    </div>
  );
}
