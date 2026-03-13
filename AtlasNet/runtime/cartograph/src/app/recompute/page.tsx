'use client';

import { useEffect, useMemo, useState } from 'react';
import type { RecomputeSnapshotTelemetry } from '../shared/cartographTypes';
import {
  useHeuristicShapes,
  useRecomputeSnapshots,
} from '../shared/useTelemetryPolling';
import { RecomputeMapViewport } from './RecomputeMapViewport';
import {
  buildRecomputeOverlayLayers,
  type RecomputeOverlayLayer,
} from './recomputeOverlayShapes';
import type { ShapeJS } from '../shared/cartographTypes';

const POLL_INTERVAL_MS = 1000;

interface RecomputeCycleGroup {
  cycleId: number;
  createdAtMs: number;
  entityCount: number;
  availableServerCount: number;
  targetHeuristicType: string | null;
  snapshots: RecomputeSnapshotTelemetry[];
}

interface CapturedBoundsSnapshot {
  cycleId: number;
  capturedAtMs: number;
  shapes: ShapeJS[];
}

function formatTimestamp(timestampMs: number): string {
  if (!Number.isFinite(timestampMs) || timestampMs <= 0) {
    return 'unknown';
  }
  return new Date(timestampMs).toLocaleTimeString();
}

function buildCycleGroups(
  snapshots: RecomputeSnapshotTelemetry[]
): RecomputeCycleGroup[] {
  const byCycleId = new Map<number, RecomputeSnapshotTelemetry[]>();
  for (const snapshot of snapshots) {
    const cycleId = snapshot.cycleId > 0 ? snapshot.cycleId : snapshot.snapshotId;
    const current = byCycleId.get(cycleId);
    if (current) {
      current.push(snapshot);
    } else {
      byCycleId.set(cycleId, [snapshot]);
    }
  }

  return Array.from(byCycleId.entries())
    .map(([cycleId, cycleSnapshots]) => {
      const sortedSnapshots = [...cycleSnapshots].sort(
        (left, right) => left.snapshotId - right.snapshotId
      );
      const representative = sortedSnapshots[0];
      return {
        cycleId,
        createdAtMs: representative?.createdAtMs ?? 0,
        entityCount: representative?.entityCount ?? 0,
        availableServerCount: representative?.availableServerCount ?? 0,
        targetHeuristicType: representative?.targetHeuristicType ?? null,
        snapshots: sortedSnapshots,
      };
    })
    .sort((left, right) => {
      if (right.createdAtMs !== left.createdAtMs) {
        return right.createdAtMs - left.createdAtMs;
      }
      return right.cycleId - left.cycleId;
    });
}

function cloneShape(shape: ShapeJS): ShapeJS {
  return {
    ...shape,
    position: { ...shape.position },
    size: shape.size ? { ...shape.size } : undefined,
    points: shape.points ? shape.points.map((point) => ({ ...point })) : undefined,
  };
}

function cloneShapes(shapes: ShapeJS[]): ShapeJS[] {
  return shapes.map(cloneShape);
}

function SnapshotJsonCard({
  snapshot,
}: {
  snapshot: RecomputeSnapshotTelemetry;
}) {
  const inputJson = useMemo(
    () => JSON.stringify(snapshot.inputJsonRaw ?? {}, null, 2),
    [snapshot.inputJsonRaw]
  );
  const outputJson = useMemo(
    () => JSON.stringify(snapshot.outputJsonRaw ?? {}, null, 2),
    [snapshot.outputJsonRaw]
  );

  return (
    <article className="rounded-3xl border border-slate-800 bg-slate-900/70 p-5">
      <div className="flex flex-wrap items-start justify-between gap-3">
        <div>
          <div className="text-xs uppercase tracking-[0.22em] text-cyan-400">
            {snapshot.recomputeHeuristic}
          </div>
          <div className="mt-1 text-sm text-slate-300">
            snapshot #{snapshot.snapshotId} | schema={snapshot.inputSchema}
          </div>
        </div>
        <div className="text-right text-xs text-slate-500">
          <div>{formatTimestamp(snapshot.createdAtMs)}</div>
          <div>target={snapshot.targetHeuristicType ?? 'unknown'}</div>
        </div>
      </div>

      <div className="mt-4 grid gap-3 sm:grid-cols-4">
        <div className="rounded-2xl border border-slate-800 bg-slate-950/70 p-3">
          <div className="text-xs uppercase tracking-wide text-slate-500">Entities</div>
          <div className="font-mono text-xl text-slate-100">{snapshot.entityCount}</div>
        </div>
        <div className="rounded-2xl border border-slate-800 bg-slate-950/70 p-3">
          <div className="text-xs uppercase tracking-wide text-slate-500">Available</div>
          <div className="font-mono text-xl text-slate-100">
            {snapshot.availableServerCount}
          </div>
        </div>
        <div className="rounded-2xl border border-slate-800 bg-slate-950/70 p-3">
          <div className="text-xs uppercase tracking-wide text-slate-500">Hotspots</div>
          <div className="font-mono text-xl text-slate-100">{snapshot.hotspotCount}</div>
        </div>
        <div className="rounded-2xl border border-slate-800 bg-slate-950/70 p-3">
          <div className="text-xs uppercase tracking-wide text-slate-500">Cycle</div>
          <div className="font-mono text-xl text-slate-100">{snapshot.cycleId}</div>
        </div>
      </div>

      <div className="mt-4 grid gap-3 xl:grid-cols-2">
        <div className="rounded-2xl border border-slate-800 bg-slate-950/80 p-3">
          <div className="mb-2 text-xs uppercase tracking-wide text-slate-500">
            INPUT_JSON
          </div>
          <pre className="overflow-x-auto text-xs leading-6 text-slate-200">
            {inputJson}
          </pre>
        </div>
        <div className="rounded-2xl border border-slate-800 bg-slate-950/80 p-3">
          <div className="mb-2 text-xs uppercase tracking-wide text-slate-500">
            OUTPUT_JSON
          </div>
          <pre className="overflow-x-auto text-xs leading-6 text-slate-200">
            {outputJson}
          </pre>
        </div>
      </div>
    </article>
  );
}

export default function RecomputePage() {
  const recompute = useRecomputeSnapshots({
    intervalMs: POLL_INTERVAL_MS,
    enabled: true,
    resetOnException: false,
    resetOnHttpError: false,
  });
  const baseShapes = useHeuristicShapes({
    intervalMs: POLL_INTERVAL_MS,
    enabled: true,
    resetOnException: false,
    resetOnHttpError: false,
  });

  const cycleGroups = useMemo(
    () => buildCycleGroups(recompute.snapshots),
    [recompute.snapshots]
  );
  const [selectedCycleId, setSelectedCycleId] = useState<number | null>(null);
  const [activeLayerIds, setActiveLayerIds] = useState<string[]>([]);
  const [capturedBoundsByCycle, setCapturedBoundsByCycle] = useState<
    Record<number, CapturedBoundsSnapshot>
  >({});

  useEffect(() => {
    if (cycleGroups.length === 0) {
      setSelectedCycleId(null);
      return;
    }
    if (
      selectedCycleId == null ||
      !cycleGroups.some((group) => group.cycleId === selectedCycleId)
    ) {
      setSelectedCycleId(cycleGroups[0].cycleId);
    }
  }, [cycleGroups, selectedCycleId]);

  useEffect(() => {
    const latestCycle = cycleGroups[0];
    if (!latestCycle || baseShapes.length === 0) {
      return;
    }

    setCapturedBoundsByCycle((previous) => {
      const existing = previous[latestCycle.cycleId];
      if (existing && existing.shapes.length > 0) {
        return previous;
      }

      return {
        ...previous,
        [latestCycle.cycleId]: {
          cycleId: latestCycle.cycleId,
          capturedAtMs: Date.now(),
          shapes: cloneShapes(baseShapes),
        },
      };
    });
  }, [baseShapes, cycleGroups]);

  const selectedCycle = useMemo(
    () => cycleGroups.find((group) => group.cycleId === selectedCycleId) ?? null,
    [cycleGroups, selectedCycleId]
  );
  const selectedBoundsSnapshot = useMemo(
    () =>
      selectedCycle ? capturedBoundsByCycle[selectedCycle.cycleId] ?? null : null,
    [capturedBoundsByCycle, selectedCycle]
  );
  const selectedBaseShapes = selectedBoundsSnapshot?.shapes ?? [];
  const overlayLayers = useMemo<RecomputeOverlayLayer[]>(
    () => (selectedCycle ? buildRecomputeOverlayLayers(selectedCycle.snapshots) : []),
    [selectedCycle]
  );

  useEffect(() => {
    setActiveLayerIds(overlayLayers.map((layer) => layer.id));
  }, [overlayLayers]);

  const activeLayerIdSet = useMemo(
    () => new Set(activeLayerIds),
    [activeLayerIds]
  );

  const toggleLayer = (layerId: string) => {
    setActiveLayerIds((previous) =>
      previous.includes(layerId)
        ? previous.filter((id) => id !== layerId)
        : [...previous, layerId]
    );
  };

  return (
    <div className="space-y-6">
      <div className="flex flex-wrap items-end justify-between gap-3">
        <div>
          <h1 className="text-2xl font-semibold text-slate-100">Recompute snapshots</h1>
          <p className="mt-1 text-sm text-slate-400">
            Bounds-first recompute viewer. It reuses the map renderer, then overlays
            heuristic-specific geometry from the snapshot payloads.
          </p>
        </div>
        <div className="rounded-2xl border border-slate-800 bg-slate-900/70 px-4 py-3 text-right">
          <div className="text-xs uppercase tracking-wide text-slate-500">Latest capture</div>
          <div className="font-mono text-2xl text-slate-100">
            {formatTimestamp(cycleGroups[0]?.createdAtMs ?? 0)}
          </div>
          <div className="text-xs text-slate-500">
            {cycleGroups[0] ? `cycle=${cycleGroups[0].cycleId}` : 'waiting'}
          </div>
        </div>
      </div>

      <div className="grid gap-6 xl:grid-cols-[340px_minmax(0,1fr)]">
        <aside className="rounded-3xl border border-slate-800 bg-slate-900/60 overflow-hidden">
          <div className="border-b border-slate-800 px-4 py-3">
            <div className="text-xs uppercase tracking-[0.22em] text-slate-500">
              Captures
            </div>
            <div className="mt-1 text-sm text-slate-300">
              Select a recompute cycle to inspect its frozen bounds capture and
              overlay payloads.
            </div>
          </div>
          {cycleGroups.length > 0 ? (
            <div className="divide-y divide-slate-800">
              {cycleGroups.map((group) => {
                const selected = group.cycleId === selectedCycleId;
                return (
                  <button
                    key={group.cycleId}
                    type="button"
                    onClick={() => setSelectedCycleId(group.cycleId)}
                    className={
                      'w-full px-4 py-4 text-left transition ' +
                      (selected
                        ? 'bg-cyan-500/10'
                        : 'hover:bg-slate-900/70')
                    }
                  >
                    <div className="flex items-start justify-between gap-3">
                      <div>
                        <div className="text-xs uppercase tracking-[0.22em] text-cyan-400">
                          Cycle {group.cycleId}
                        </div>
                        <div className="mt-1 text-sm text-slate-200">
                          {group.entityCount} entities | k={group.availableServerCount}
                        </div>
                        <div className="mt-1 text-xs text-slate-500">
                          {group.snapshots
                            .map((snapshot) => snapshot.recomputeHeuristic)
                            .join(' • ')}
                        </div>
                      </div>
                      <div className="text-right text-xs text-slate-500">
                        <div>{formatTimestamp(group.createdAtMs)}</div>
                        <div>{group.targetHeuristicType ?? 'unknown'}</div>
                      </div>
                    </div>
                  </button>
                );
              })}
            </div>
          ) : (
            <div className="px-4 py-10 text-sm text-slate-500">
              Waiting for recompute snapshots from the backend service.
            </div>
          )}
        </aside>

        <div className="space-y-6">
          {selectedCycle ? (
            <>
              <RecomputeMapViewport
                baseShapes={selectedBaseShapes}
                overlayLayers={overlayLayers}
                activeLayerIds={activeLayerIdSet}
                hasHistoricalBounds={selectedBoundsSnapshot != null}
                boundsSnapshotLabel={
                  selectedBoundsSnapshot
                    ? `Captured at ${formatTimestamp(selectedBoundsSnapshot.capturedAtMs)}`
                    : 'No historical bounds were captured for this cycle.'
                }
              />

              {overlayLayers.length > 0 ? (
                <div className="rounded-3xl border border-slate-800 bg-slate-900/60 p-4">
                  <div className="text-xs uppercase tracking-[0.22em] text-slate-500">
                    Overlay Toggles
                  </div>
                  <div className="mt-4 flex flex-wrap gap-3">
                    {overlayLayers.map((layer) => {
                      const active = activeLayerIdSet.has(layer.id);
                      return (
                        <button
                          key={layer.id}
                          type="button"
                          onClick={() => toggleLayer(layer.id)}
                          className={
                            'rounded-2xl border px-3 py-2 text-left text-sm transition ' +
                            (active
                              ? 'border-cyan-500 bg-cyan-500/10 text-cyan-100'
                              : 'border-slate-700 bg-slate-950/40 text-slate-300 hover:bg-slate-800')
                          }
                        >
                          <div>{layer.title}</div>
                          <div className="mt-1 text-xs opacity-80">
                            {layer.subtitle} | {layer.shapeCount}
                          </div>
                        </button>
                      );
                    })}
                  </div>
                </div>
              ) : null}

              <div className="space-y-4">
                {selectedCycle.snapshots.map((snapshot) => (
                  <SnapshotJsonCard
                    key={snapshot.snapshotId}
                    snapshot={snapshot}
                  />
                ))}
              </div>
            </>
          ) : (
            <div className="rounded-3xl border border-dashed border-slate-800 px-6 py-16 text-center text-sm text-slate-500">
              Select a capture to inspect the map snapshot and its heuristic overlays.
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
