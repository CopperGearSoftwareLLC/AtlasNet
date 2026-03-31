'use client';

import { useEffect, useMemo, useRef, useState } from 'react';
import type {
  HeuristicControlState,
  RecomputeSnapshotTelemetry,
  ShapeJS,
} from '../shared/cartographTypes';
import {
  useHeuristicShapes,
  useRecomputeSnapshots,
} from '../shared/useTelemetryPolling';
import { RecomputeMapViewport } from './RecomputeMapViewport';
import {
  buildRecomputeOverlayLayers,
  type RecomputeOverlayLayer,
} from './recomputeOverlayShapes';

const POLL_INTERVAL_MS = 1000;
const HEURISTIC_CONTROL_POLL_INTERVAL_MS = 5000;
const MIN_RECOMPUTE_INTERVAL_MS = 100;

const EMPTY_HEURISTIC_CONTROL: HeuristicControlState = {
  currentHeuristicType: null,
  desiredHeuristicType: null,
  allowedHeuristicTypes: [],
  recomputeMode: 'interval',
  recomputeIntervalMs: 5000,
  loadState: 'stubbed',
};

interface RecomputeCycleGroup {
  cycleId: number;
  createdAtMs: number;
  entityCount: number;
  availableServerCount: number;
  targetHeuristicType: string | null;
  snapshots: RecomputeSnapshotTelemetry[];
  captureSource: 'backend' | 'local';
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
        captureSource: 'backend',
      };
    })
    .sort((left, right) => {
      if (right.createdAtMs !== left.createdAtMs) {
        return right.createdAtMs - left.createdAtMs;
      }
      return right.cycleId - left.cycleId;
    });
}

function isVoronoiTarget(type: string | null): boolean {
  return (
    type === 'Voronoi' ||
    type === 'HotspotVoronoi' ||
    type === 'LlmVoronoi'
  );
}

function cloneShape(shape: ShapeJS): ShapeJS {
  return {
    ...shape,
    position: { ...shape.position },
    size: shape.size ? { ...shape.size } : undefined,
    points: shape.points ? shape.points.map((point) => ({ ...point })) : undefined,
    site: shape.site ? { ...shape.site } : undefined,
    halfPlanes: shape.halfPlanes
      ? shape.halfPlanes.map((plane) => ({ ...plane }))
      : undefined,
  };
}

function cloneShapes(shapes: ShapeJS[]): ShapeJS[] {
  return shapes.map(cloneShape);
}

function createShapeFingerprint(
  shapes: ShapeJS[],
  heuristicType: string | null
): string {
  return JSON.stringify({
    heuristicType,
    shapes: shapes.map((shape) => ({
      id: shape.id ?? null,
      ownerId: shape.ownerId ?? null,
      type: shape.type,
      position: shape.position,
      radius: shape.radius ?? null,
      size: shape.size ?? null,
      points: shape.points ?? null,
      site: shape.site ?? null,
      halfPlanes: shape.halfPlanes ?? null,
      color: shape.color ?? null,
      label: shape.label ?? null,
    })),
  });
}

function SnapshotJsonCard({
  snapshot,
}: {
  snapshot: RecomputeSnapshotTelemetry;
}) {
  const inputJson = useMemo(
    () => {
      const raw = snapshot.inputJsonRaw ?? {};
      if (
        raw &&
        typeof raw === 'object' &&
        !Array.isArray(raw) &&
        typeof raw.prompt === 'string' &&
        raw.prompt.trim().length > 0
      ) {
        return raw.prompt;
      }
      return JSON.stringify(raw, null, 2);
    },
    [snapshot.inputJsonRaw]
  );
  const outputJson = useMemo(() => {
    const raw = snapshot.outputJsonRaw ?? {};
    if (
      raw &&
      typeof raw === 'object' &&
      !Array.isArray(raw) &&
      raw.model_completion_json &&
      typeof raw.model_completion_json === 'object'
    ) {
      return JSON.stringify(raw.model_completion_json, null, 2);
    }
    if (
      raw &&
      typeof raw === 'object' &&
      !Array.isArray(raw) &&
      typeof raw.model_completion_raw === 'string' &&
      raw.model_completion_raw.trim().length > 0
    ) {
      return raw.model_completion_raw;
    }
    return JSON.stringify(raw, null, 2);
  }, [snapshot.outputJsonRaw]);

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
          {snapshot.seedSource || snapshot.inferenceNote ? (
            <div className="mt-2 flex flex-wrap gap-2 text-xs">
              {snapshot.seedSource ? (
                <span className="rounded-full border border-emerald-500/40 bg-emerald-500/10 px-2 py-1 text-emerald-200">
                  source={snapshot.seedSource}
                </span>
              ) : null}
              {snapshot.inferenceNote ? (
                <span className="rounded-full border border-slate-700 bg-slate-950/70 px-2 py-1 text-slate-300">
                  note={snapshot.inferenceNote}
                </span>
              ) : null}
              {snapshot.endpoint ? (
                <span className="rounded-full border border-slate-700 bg-slate-950/70 px-2 py-1 text-slate-300">
                  endpoint={snapshot.endpoint}
                </span>
              ) : null}
              {snapshot.modelId ? (
                <span className="rounded-full border border-slate-700 bg-slate-950/70 px-2 py-1 text-slate-300">
                  model={snapshot.modelId}
                </span>
              ) : null}
            </div>
          ) : null}
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
            INPUT
          </div>
          <pre className="overflow-x-auto text-xs leading-6 text-slate-200">
            {inputJson}
          </pre>
        </div>
        <div className="rounded-2xl border border-slate-800 bg-slate-950/80 p-3">
          <div className="mb-2 text-xs uppercase tracking-wide text-slate-500">
            OUTPUT
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
  const [layerVisibilityById, setLayerVisibilityById] = useState<
    Record<string, boolean>
  >({});
  const [hasUserPinnedSelection, setHasUserPinnedSelection] = useState(false);
  const [capturedBoundsByCycle, setCapturedBoundsByCycle] = useState<
    Record<number, CapturedBoundsSnapshot>
  >({});
  const [observedCycleGroups, setObservedCycleGroups] = useState<RecomputeCycleGroup[]>([]);
  const [heuristicControl, setHeuristicControl] =
    useState<HeuristicControlState>(EMPTY_HEURISTIC_CONTROL);
  const [selectedHeuristicType, setSelectedHeuristicType] = useState('');
  const [selectedRecomputeMode, setSelectedRecomputeMode] = useState<
    HeuristicControlState['recomputeMode']
  >('interval');
  const [selectedRecomputeIntervalMs, setSelectedRecomputeIntervalMs] = useState('5000');
  const [heuristicControlBusy, setHeuristicControlBusy] = useState(false);
  const [heuristicControlMessage, setHeuristicControlMessage] =
    useState<string | null>(null);
  const lastObservedFingerprintRef = useRef<string | null>(null);
  const nextObservedCycleIdRef = useRef(1);
  const recomputeControlDraftDirtyRef = useRef(false);

  useEffect(() => {
    let alive = true;

    async function pollHeuristicControl() {
      try {
        const response = await fetch('/api/heuristic-control', {
          cache: 'no-store',
        });
        if (!response.ok) {
          return;
        }

        const payload = (await response.json()) as HeuristicControlState;
        if (!alive) {
          return;
        }

        setHeuristicControl(payload);
        setSelectedHeuristicType((previous) => {
          if (
            previous &&
            Array.isArray(payload.allowedHeuristicTypes) &&
            payload.allowedHeuristicTypes.includes(previous)
          ) {
            return previous;
          }
          return payload.desiredHeuristicType ?? payload.currentHeuristicType ?? '';
        });
        if (!recomputeControlDraftDirtyRef.current) {
          setSelectedRecomputeMode(payload.recomputeMode);
          setSelectedRecomputeIntervalMs(String(payload.recomputeIntervalMs));
        }
      } catch {}
    }

    void pollHeuristicControl();
    const intervalId = setInterval(() => {
      void pollHeuristicControl();
    }, HEURISTIC_CONTROL_POLL_INTERVAL_MS);

    return () => {
      alive = false;
      clearInterval(intervalId);
    };
  }, []);

  useEffect(() => {
    const currentHeuristicType = heuristicControl.currentHeuristicType;
    if (!currentHeuristicType || baseShapes.length === 0) {
      return;
    }

    const nextFingerprint = createShapeFingerprint(baseShapes, currentHeuristicType);
    if (lastObservedFingerprintRef.current === nextFingerprint) {
      return;
    }

    lastObservedFingerprintRef.current = nextFingerprint;
    const capturedAtMs = Date.now();
    const cycleId = nextObservedCycleIdRef.current++;
    const capturedShapes = cloneShapes(baseShapes);

    setCapturedBoundsByCycle((previous) => ({
      ...previous,
      [cycleId]: {
        cycleId,
        capturedAtMs,
        shapes: capturedShapes,
      },
    }));
    setObservedCycleGroups((previous) =>
      [
        {
          cycleId,
          createdAtMs: capturedAtMs,
          entityCount: 0,
          availableServerCount: 0,
          targetHeuristicType: currentHeuristicType,
          snapshots: [],
          captureSource: 'local',
        },
        ...previous,
      ].slice(0, 64)
    );
  }, [baseShapes, heuristicControl.currentHeuristicType]);

  const displayCycleGroups = isVoronoiTarget(heuristicControl.currentHeuristicType ?? null)
    ? cycleGroups
    : observedCycleGroups;

  useEffect(() => {
    if (displayCycleGroups.length === 0) {
      setSelectedCycleId(null);
      setHasUserPinnedSelection(false);
      return;
    }

    const selectedStillExists = displayCycleGroups.some(
      (group) => group.cycleId === selectedCycleId
    );
    if (selectedCycleId != null && selectedStillExists) {
      return;
    }

    if (hasUserPinnedSelection && selectedCycleId != null && !selectedStillExists) {
      setHasUserPinnedSelection(false);
    }

    if (!hasUserPinnedSelection || selectedCycleId == null || !selectedStillExists) {
      setSelectedCycleId(displayCycleGroups[0].cycleId);
    }
  }, [displayCycleGroups, hasUserPinnedSelection, selectedCycleId]);

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
    () =>
      displayCycleGroups.find((group) => group.cycleId === selectedCycleId) ?? null,
    [displayCycleGroups, selectedCycleId]
  );
  const selectedBoundsSnapshot = useMemo(
    () =>
      selectedCycle ? capturedBoundsByCycle[selectedCycle.cycleId] ?? null : null,
    [capturedBoundsByCycle, selectedCycle]
  );
  const overlayLayers = useMemo<RecomputeOverlayLayer[]>(
    () => (selectedCycle ? buildRecomputeOverlayLayers(selectedCycle.snapshots) : []),
    [selectedCycle]
  );
  const selectedUsesCellBounds = isVoronoiTarget(selectedCycle?.targetHeuristicType ?? null);
  const selectedBaseShapes =
    selectedUsesCellBounds || !selectedBoundsSnapshot
      ? []
      : selectedBoundsSnapshot.shapes;
  const boundsStatusText = selectedUsesCellBounds
    ? 'Voronoi bounds are rendered through the Cells overlay for this cycle.'
    : selectedBoundsSnapshot
      ? `Map bounds captured at ${formatTimestamp(selectedBoundsSnapshot.capturedAtMs)}`
      : 'No map-bounds snapshot was captured for this cycle in the current session.';

  const activeLayerIdSet = useMemo(
    () =>
      new Set(
        overlayLayers
          .filter((layer) => layerVisibilityById[layer.id] !== false)
          .map((layer) => layer.id)
      ),
    [layerVisibilityById, overlayLayers]
  );

  const toggleLayer = (layerId: string) => {
    setLayerVisibilityById((previous) => ({
      ...previous,
      [layerId]: previous[layerId] === false,
    }));
  };

  async function applyHeuristicControl() {
    if (!selectedHeuristicType || heuristicControlBusy) {
      return;
    }

    const nextRecomputeIntervalMs = Math.max(
      MIN_RECOMPUTE_INTERVAL_MS,
      Math.round(Number(selectedRecomputeIntervalMs) || heuristicControl.recomputeIntervalMs)
    );

    setHeuristicControlBusy(true);
    setHeuristicControlMessage(null);

    try {
      const response = await fetch('/api/heuristic-control', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          heuristicType: selectedHeuristicType,
          recomputeMode: selectedRecomputeMode,
          recomputeIntervalMs: nextRecomputeIntervalMs,
        }),
      });
      if (!response.ok) {
        setHeuristicControlMessage(`Set failed (${response.status}).`);
        return;
      }

      const payload = (await response.json()) as HeuristicControlState;
      setHeuristicControl(payload);
      setSelectedHeuristicType(
        payload.desiredHeuristicType ?? payload.currentHeuristicType ?? ''
      );
      recomputeControlDraftDirtyRef.current = false;
      setSelectedRecomputeMode(payload.recomputeMode);
      setSelectedRecomputeIntervalMs(String(payload.recomputeIntervalMs));
      setHeuristicControlMessage(
        payload.recomputeMode === 'load'
          ? 'Heuristic control updated. Load mode is stubbed out in cartograph.'
          : 'Heuristic control updated.'
      );
    } catch {
      setHeuristicControlMessage('Set failed.');
    } finally {
      setHeuristicControlBusy(false);
    }
  }

  async function requestManualRecompute() {
    if (heuristicControlBusy) {
      return;
    }

    setHeuristicControlBusy(true);
    setHeuristicControlMessage(null);

    try {
      const response = await fetch('/api/heuristic-control', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          requestRecompute: true,
        }),
      });
      if (!response.ok) {
        setHeuristicControlMessage(`Manual recompute failed (${response.status}).`);
        return;
      }

      const payload = (await response.json()) as HeuristicControlState;
      setHeuristicControl(payload);
      setSelectedRecomputeMode(payload.recomputeMode);
      setSelectedRecomputeIntervalMs(String(payload.recomputeIntervalMs));
      setHeuristicControlMessage('Manual recompute requested.');
    } catch {
      setHeuristicControlMessage('Manual recompute failed.');
    } finally {
      setHeuristicControlBusy(false);
    }
  }

  const selectCycle = (cycleId: number) => {
    setHasUserPinnedSelection(true);
    setSelectedCycleId(cycleId);
  };

  return (
    <div className="space-y-6">
      <div className="flex flex-wrap items-end justify-between gap-3">
        <div>
          <h1 className="text-2xl font-semibold text-slate-100">Recompute snapshots</h1>
          <p className="mt-1 text-sm text-slate-400">
            Recompute viewer. It snapshots the Map page bounds for each observed
            cycle, then layers heuristic-specific overlay data on top.
          </p>
        </div>
        <div className="rounded-2xl border border-slate-800 bg-slate-900/70 px-4 py-3 text-right">
          <div className="text-xs uppercase tracking-wide text-slate-500">Latest capture</div>
          <div className="font-mono text-2xl text-slate-100">
            {formatTimestamp(displayCycleGroups[0]?.createdAtMs ?? 0)}
          </div>
          <div className="text-xs text-slate-500">
            {displayCycleGroups[0]
              ? `cycle=${displayCycleGroups[0].cycleId}`
              : 'waiting'}
          </div>
        </div>
      </div>

      <div className="grid gap-6 xl:grid-cols-[340px_minmax(0,1fr)]">
        <aside className="rounded-3xl border border-slate-800 bg-slate-900/60 overflow-hidden">
          <div className="border-b border-slate-800 px-4 py-3">
            <div className="text-xs uppercase tracking-[0.22em] text-slate-500">
              Captures
            </div>
          </div>
          {displayCycleGroups.length > 0 ? (
            <div className="divide-y divide-slate-800">
              {displayCycleGroups.map((group) => {
                const selected = group.cycleId === selectedCycleId;
                return (
                  <button
                    key={group.cycleId}
                    type="button"
                    onClick={() => selectCycle(group.cycleId)}
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
                          {group.snapshots.length > 0
                            ? `${group.entityCount} entities | k=${group.availableServerCount}`
                            : `${group.targetHeuristicType ?? 'unknown'} bounds snapshot`}
                        </div>
                        <div className="mt-1 text-xs text-slate-500">
                          {group.snapshots.length > 0
                            ? group.snapshots
                                .map((snapshot) => snapshot.recomputeHeuristic)
                                .join(' • ')
                            : 'bounds_snapshot'}
                        </div>
                      </div>
                      <div className="text-right text-xs text-slate-500">
                        <div>{formatTimestamp(group.createdAtMs)}</div>
                        <div>
                          {group.targetHeuristicType ?? 'unknown'} | {group.captureSource}
                        </div>
                      </div>
                    </div>
                  </button>
                );
              })}
            </div>
          ) : (
            <div className="px-4 py-10 text-sm text-slate-500">
              Waiting for heuristic bounds or recompute snapshots to appear.
            </div>
          )}
        </aside>

        <div className="space-y-6">
          <section className="rounded-3xl border border-slate-800 bg-slate-900/60 p-4">
            <div className="flex flex-wrap items-end justify-between gap-3">
              <div>
                <div className="text-xs uppercase tracking-[0.22em] text-slate-500">
                  Heuristic Control
                </div>
                <div className="mt-1 text-sm text-slate-300">
                  Request a runtime swap. Watchdog applies it on the next recompute cycle.
                </div>
              </div>
              <div className="text-xs text-slate-500">
                current={heuristicControl.currentHeuristicType ?? 'unknown'} | desired=
                {heuristicControl.desiredHeuristicType ?? 'unknown'} | mode=
                {heuristicControl.recomputeMode}
              </div>
            </div>

            <div className="mt-4 flex flex-wrap items-center gap-3">
              <select
                value={selectedHeuristicType}
                onChange={(event) => setSelectedHeuristicType(event.target.value)}
                className="rounded-xl border border-slate-700 bg-slate-950 px-3 py-2 text-sm text-slate-100"
              >
                {(heuristicControl.allowedHeuristicTypes.length > 0
                  ? heuristicControl.allowedHeuristicTypes
                  : ['Voronoi', 'HotspotVoronoi', 'LlmVoronoi']
                ).map((type) => (
                  <option key={type} value={type}>
                    {type}
                  </option>
                ))}
              </select>
              <button
                type="button"
                onClick={() => {
                  void applyHeuristicControl();
                }}
                disabled={!selectedHeuristicType || heuristicControlBusy}
                className="rounded-xl border border-cyan-500/40 bg-cyan-500/10 px-4 py-2 text-sm text-cyan-200 disabled:cursor-not-allowed disabled:opacity-50"
              >
                {heuristicControlBusy ? 'Applying...' : 'Apply'}
              </button>
            </div>

            <div className="mt-4 flex flex-wrap items-center gap-3">
              <select
                value={selectedRecomputeMode}
                onChange={(event) => {
                  recomputeControlDraftDirtyRef.current = true;
                  setSelectedRecomputeMode(
                    event.target.value as HeuristicControlState['recomputeMode']
                  );
                }}
                className="rounded-xl border border-slate-700 bg-slate-950 px-3 py-2 text-sm text-slate-100"
              >
                <option value="interval">interval</option>
                <option value="manual">manual</option>
                <option value="load">load</option>
              </select>

              {selectedRecomputeMode === 'interval' ? (
                <label className="flex items-center gap-2 rounded-xl border border-slate-800 bg-slate-950/70 px-3 py-2 text-sm text-slate-300">
                  <span>interval ms</span>
                  <input
                    type="number"
                    min={MIN_RECOMPUTE_INTERVAL_MS}
                    step={100}
                    value={selectedRecomputeIntervalMs}
                    onChange={(event) => {
                      recomputeControlDraftDirtyRef.current = true;
                      setSelectedRecomputeIntervalMs(event.target.value);
                    }}
                    className="w-28 rounded-md border border-slate-700 bg-slate-950 px-2 py-1 text-slate-100"
                  />
                </label>
              ) : null}

              {selectedRecomputeMode === 'manual' ? (
                <button
                  type="button"
                  onClick={() => {
                    void requestManualRecompute();
                  }}
                  disabled={heuristicControlBusy}
                  className="rounded-xl border border-emerald-500/40 bg-emerald-500/10 px-4 py-2 text-sm text-emerald-200 disabled:cursor-not-allowed disabled:opacity-50"
                >
                  recompute
                </button>
              ) : null}

              {selectedRecomputeMode === 'load' ? (
                <span className="rounded-xl border border-amber-500/30 bg-amber-500/10 px-3 py-2 text-sm text-amber-200">
                  load mode is stubbed out in cartograph
                </span>
              ) : null}
            </div>

            <span className="mt-3 block text-xs text-slate-500">
              {heuristicControlMessage ??
                'Interval runs automatically, manual waits for recompute, and load is reserved for a future cartograph flow.'}
            </span>
          </section>

          {selectedCycle ? (
            <>
              <RecomputeMapViewport
                baseShapes={selectedBaseShapes}
                overlayLayers={overlayLayers}
                activeLayerIds={activeLayerIdSet}
                boundsStatusText={boundsStatusText}
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

              {selectedCycle.snapshots.length > 0 ? (
                <div className="space-y-4">
                  {selectedCycle.snapshots.map((snapshot) => (
                    <SnapshotJsonCard
                      key={snapshot.snapshotId}
                      snapshot={snapshot}
                    />
                  ))}
                </div>
              ) : (
                <article className="rounded-3xl border border-slate-800 bg-slate-900/70 p-5">
                  <div className="text-xs uppercase tracking-[0.22em] text-cyan-400">
                    bounds_snapshot
                  </div>
                  <div className="mt-2 text-sm text-slate-300">
                    This heuristic does not currently publish recompute payload JSON, so
                    Recompute is showing the captured map bounds for this observed state.
                  </div>
                  <div className="mt-3 text-xs text-slate-500">
                    target={selectedCycle.targetHeuristicType ?? 'unknown'} | source=
                    {selectedCycle.captureSource} | captured=
                    {formatTimestamp(selectedCycle.createdAtMs)}
                  </div>
                </article>
              )}
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
