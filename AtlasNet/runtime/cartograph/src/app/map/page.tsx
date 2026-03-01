'use client';

import { useEffect, useMemo, useRef, useState } from 'react';
import type {
  AuthorityEntityTelemetry,
  AuthorityLinkMode,
  ShapeJS,
  ShardPlacementTelemetry,
  ShardTelemetry,
  TransferManifestTelemetry,
} from '../lib/cartographTypes';
import { useMapDerivedData } from '../lib/hooks/useMapDerivedData';
import { useShardHoverState } from '../lib/hooks/useShardHoverState';
import {
  useAuthorityEntities,
  useHeuristicShapes,
  useNetworkTelemetry,
  useShardPlacement,
  useTransferManifest,
} from '../lib/hooks/useTelemetryFeeds';
import {
  createMapRenderer,
  type MapProjectionMode,
  type MapViewMode,
} from '../lib/mapRenderer';
import { EntityInspectorPanel } from './entityInspector/EntityInspectorPanel';
import {
  useEntityDatabaseDetails,
  type EntityDatabaseDetailsState,
} from './entityInspector/useEntityDatabaseDetails';
import { useCtrlDragEntitySelection } from './entityInspector/useCtrlDragEntitySelection';
import { MapHud } from './components/MapHud';
import { MapPlaybackBar } from './components/MapPlaybackBar';
import { ShardHoverTooltip } from './components/ShardHoverTooltip';

const DEFAULT_POLL_INTERVAL_MS = 50;
const MIN_POLL_INTERVAL_MS = 1;
const MAX_POLL_INTERVAL_MS = 1000;
const POLL_DISABLED_AT_MS = MAX_POLL_INTERVAL_MS;
const DEFAULT_ENTITY_DETAIL_POLL_INTERVAL_MS = 1000;
const MIN_ENTITY_DETAIL_POLL_INTERVAL_MS = 250;
const MAX_ENTITY_DETAIL_POLL_INTERVAL_MS = 5000;
const ENTITY_DETAIL_POLL_DISABLED_AT_MS = MAX_ENTITY_DETAIL_POLL_INTERVAL_MS;
const DEFAULT_INTERACTION_SENSITIVITY = 1;
const SNAPSHOT_WINDOW_MS = 30_000;
const SNAPSHOT_RECORD_SPACING_MS = 10;
const PLAYBACK_TICK_INTERVAL_MS = 16;

const EMPTY_DETAILS_STATE: EntityDatabaseDetailsState = {
  loading: false,
  error: null,
  data: null,
};

interface PlaybackFrame {
  capturedAtMs: number;
  baseShapes: ShapeJS[];
  networkTelemetry: ShardTelemetry[];
  authorityEntities: AuthorityEntityTelemetry[];
  transferManifest: TransferManifestTelemetry[];
  shardPlacement: ShardPlacementTelemetry[];
  entityDetailsByEntityId: Record<string, EntityDatabaseDetailsState>;
}

function clonePlaybackPayload(
  payload: Omit<PlaybackFrame, 'capturedAtMs'>
): Omit<PlaybackFrame, 'capturedAtMs'> {
  if (typeof structuredClone === 'function') {
    return structuredClone(payload);
  }
  return JSON.parse(JSON.stringify(payload)) as Omit<PlaybackFrame, 'capturedAtMs'>;
}

function getFrameIndexAtCursor(frames: PlaybackFrame[], cursorMs: number): number {
  if (frames.length === 0) {
    return -1;
  }

  let lo = 0;
  let hi = frames.length - 1;
  let best = 0;

  while (lo <= hi) {
    const mid = (lo + hi) >> 1;
    const value = frames[mid].capturedAtMs;
    if (value <= cursorMs) {
      best = mid;
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }

  return best;
}

function getFrameAtCursor(
  frames: PlaybackFrame[],
  cursorMs: number
): PlaybackFrame | null {
  const index = getFrameIndexAtCursor(frames, cursorMs);
  if (index < 0) {
    return null;
  }
  return frames[index] ?? null;
}

function snapshotDetailsByEntityId(
  cache: Map<string, EntityDatabaseDetailsState>
): Record<string, EntityDatabaseDetailsState> {
  const out: Record<string, EntityDatabaseDetailsState> = {};
  for (const [entityId, state] of cache.entries()) {
    out[entityId] = state;
  }
  return out;
}

export default function MapPage() {
  const containerRef = useRef<HTMLDivElement>(null);
  const rendererRef = useRef<ReturnType<typeof createMapRenderer> | null>(null);
  const historyRef = useRef<PlaybackFrame[]>([]);
  const playbackLastTickMsRef = useRef<number | null>(null);
  const [showGnsConnections, setShowGnsConnections] = useState(true);
  const [showAuthorityEntities, setShowAuthorityEntities] = useState(true);
  const [authorityLinkMode, setAuthorityLinkMode] =
    useState<AuthorityLinkMode>('owner');
  const [showShardHoverDetails, setShowShardHoverDetails] = useState(true);
  const [hoveredShardId, setHoveredShardId] = useState<string | null>(null);
  const [hoveredShardAnchor, setHoveredShardAnchor] = useState<{
    x: number;
    y: number;
  } | null>(null);
  const [viewMode, setViewMode] = useState<MapViewMode>('2d');
  const [projectionMode, setProjectionMode] =
    useState<MapProjectionMode>('orthographic');
  const [interactionSensitivity, setInteractionSensitivity] = useState(
    DEFAULT_INTERACTION_SENSITIVITY
  );
  const [pollIntervalMs, setPollIntervalMs] = useState(DEFAULT_POLL_INTERVAL_MS);
  const [entityDetailPollIntervalMs, setEntityDetailPollIntervalMs] = useState(
    DEFAULT_ENTITY_DETAIL_POLL_INTERVAL_MS
  );
  const [playbackFrames, setPlaybackFrames] = useState<PlaybackFrame[] | null>(
    null
  );
  const [playbackCursorMs, setPlaybackCursorMs] = useState(0);
  const [playbackPaused, setPlaybackPaused] = useState(true);
  const [playbackDirection, setPlaybackDirection] = useState<1 | -1>(1);

  const telemetryPollIntervalMs =
    pollIntervalMs >= POLL_DISABLED_AT_MS ? 0 : pollIntervalMs;
  const baseShapesLive = useHeuristicShapes({
    intervalMs: telemetryPollIntervalMs,
    resetOnException: true,
    resetOnHttpError: false,
  });
  const networkTelemetryLive = useNetworkTelemetry({
    intervalMs: telemetryPollIntervalMs,
    resetOnException: true,
    resetOnHttpError: false,
  });
  const authorityEntitiesLive = useAuthorityEntities({
    intervalMs: telemetryPollIntervalMs,
    resetOnException: true,
    resetOnHttpError: false,
  });
  const transferManifestLive = useTransferManifest({
    intervalMs: telemetryPollIntervalMs,
    resetOnException: true,
    resetOnHttpError: false,
  });
  const shardPlacementLive = useShardPlacement({
    intervalMs:
      telemetryPollIntervalMs <= 0 ? 0 : Math.max(1000, telemetryPollIntervalMs),
    enabled: true,
    resetOnException: false,
    resetOnHttpError: false,
  });

  const playbackStartMs =
    playbackFrames && playbackFrames.length > 0
      ? playbackFrames[0].capturedAtMs
      : 0;
  const playbackEndMs =
    playbackFrames && playbackFrames.length > 0
      ? playbackFrames[playbackFrames.length - 1].capturedAtMs
      : 0;
  const playbackFrame = useMemo(
    () =>
      playbackFrames && playbackFrames.length > 0
        ? getFrameAtCursor(playbackFrames, playbackCursorMs)
        : null,
    [playbackFrames, playbackCursorMs]
  );
  const playbackActive = playbackFrame != null;

  const authorityEntitiesForSelection =
    playbackFrame?.authorityEntities ?? authorityEntitiesLive;
  const {
    selectedEntities,
    activeEntityId,
    hoveredEntityId: hoveredSelectedEntityId,
    selectionRect,
    clearSelection,
    onPointerDownCapture,
    onPointerMoveCapture,
    onPointerUpCapture,
    onPointerCancelCapture,
    setActiveEntityId,
    setHoveredEntityId,
  } = useCtrlDragEntitySelection({
    containerRef,
    rendererRef,
    entities: authorityEntitiesForSelection,
  });
  const activeSelectedEntity = useMemo(
    () =>
      selectedEntities.find((entity) => entity.entityId === activeEntityId) ?? null,
    [activeEntityId, selectedEntities]
  );

  const activeEntityLive = useMemo(
    () =>
      authorityEntitiesLive.find((entity) => entity.entityId === activeEntityId) ??
      null,
    [activeEntityId, authorityEntitiesLive]
  );
  const liveEntityDetails = useEntityDatabaseDetails(
    activeEntityLive,
    entityDetailPollIntervalMs
  );
  const playbackLookupDetails = useEntityDatabaseDetails(
    playbackActive ? activeSelectedEntity : null,
    0
  );
  const liveEntityDetailsCacheRef = useRef<Map<string, EntityDatabaseDetailsState>>(
    new Map()
  );

  useEffect(() => {
    const entityId = activeEntityLive?.entityId ?? '';
    if (!entityId) {
      return;
    }

    const previous = liveEntityDetailsCacheRef.current.get(entityId) ?? null;
    liveEntityDetailsCacheRef.current.set(entityId, {
      loading: liveEntityDetails.loading,
      error: liveEntityDetails.error,
      data: liveEntityDetails.data ?? previous?.data ?? null,
    });
  }, [activeEntityLive, liveEntityDetails]);

  const latestLiveFrameDataRef = useRef<Omit<PlaybackFrame, 'capturedAtMs'> | null>(
    null
  );

  useEffect(() => {
    latestLiveFrameDataRef.current = {
      baseShapes: baseShapesLive,
      networkTelemetry: networkTelemetryLive,
      authorityEntities: authorityEntitiesLive,
      transferManifest: transferManifestLive,
      shardPlacement: shardPlacementLive,
      entityDetailsByEntityId: snapshotDetailsByEntityId(
        liveEntityDetailsCacheRef.current
      ),
    };
  }, [
    baseShapesLive,
    networkTelemetryLive,
    authorityEntitiesLive,
    transferManifestLive,
    shardPlacementLive,
    liveEntityDetails,
    activeEntityLive,
  ]);

  useEffect(() => {
    const intervalId = setInterval(() => {
      const latest = latestLiveFrameDataRef.current;
      if (!latest) {
        return;
      }

      const nowMs = Date.now();
      const frozen = clonePlaybackPayload(latest);
      const frame: PlaybackFrame = {
        capturedAtMs: nowMs,
        ...frozen,
      };
      const frames = historyRef.current;
      frames.push(frame);

      const cutoffMs = nowMs - SNAPSHOT_WINDOW_MS;
      while (frames.length > 0 && frames[0].capturedAtMs < cutoffMs) {
        frames.shift();
      }
    }, SNAPSHOT_RECORD_SPACING_MS);

    return () => {
      clearInterval(intervalId);
    };
  }, []);

  useEffect(() => {
    if (!playbackActive || playbackPaused) {
      playbackLastTickMsRef.current = null;
      return;
    }

    const intervalId = setInterval(() => {
      const now = performance.now();
      const previousTick = playbackLastTickMsRef.current ?? now;
      playbackLastTickMsRef.current = now;
      const elapsedMs = Math.max(0, now - previousTick);

      setPlaybackCursorMs((prev) => {
        const next = prev + playbackDirection * elapsedMs;
        if (playbackDirection < 0) {
          return Math.max(playbackStartMs, next);
        }
        return Math.min(playbackEndMs, next);
      });
    }, PLAYBACK_TICK_INTERVAL_MS);

    return () => {
      clearInterval(intervalId);
    };
  }, [
    playbackActive,
    playbackDirection,
    playbackEndMs,
    playbackPaused,
    playbackStartMs,
  ]);

  useEffect(() => {
    if (!playbackActive || playbackPaused) {
      return;
    }
    if (
      playbackCursorMs <= playbackStartMs ||
      playbackCursorMs >= playbackEndMs
    ) {
      setPlaybackPaused(true);
    }
  }, [
    playbackActive,
    playbackCursorMs,
    playbackEndMs,
    playbackPaused,
    playbackStartMs,
  ]);

  const playbackEntityDetails =
    playbackActive && activeEntityId
      ? playbackFrame?.entityDetailsByEntityId[activeEntityId] ?? null
      : null;
  const entityDetails = playbackActive
    ? playbackEntityDetails ?? playbackLookupDetails
    : liveEntityDetails;

  function takeSnapshot(): void {
    const frames = historyRef.current;
    if (frames.length === 0) {
      return;
    }
    const snapshot = [...frames];
    const endMs = snapshot[snapshot.length - 1].capturedAtMs;
    setPlaybackFrames(snapshot);
    setPlaybackCursorMs(endMs);
    setPlaybackDirection(1);
    setPlaybackPaused(true);
    playbackLastTickMsRef.current = null;
  }

  function resumeLive(): void {
    setPlaybackFrames(null);
    setPlaybackPaused(true);
    playbackLastTickMsRef.current = null;
  }

  function seekPlayback(nextCursorMs: number): void {
    if (!playbackActive) {
      return;
    }
    const minMs = playbackStartMs;
    const maxMs = playbackEndMs;
    const clamped = Math.max(minMs, Math.min(maxMs, nextCursorMs));
    setPlaybackCursorMs(clamped);
    playbackLastTickMsRef.current = null;
  }

  function playForward(): void {
    if (!playbackActive) {
      return;
    }
    setPlaybackDirection(1);
    setPlaybackPaused(false);
    playbackLastTickMsRef.current = null;
  }

  function playReverse(): void {
    if (!playbackActive) {
      return;
    }
    setPlaybackDirection(-1);
    setPlaybackPaused(false);
    playbackLastTickMsRef.current = null;
  }

  function pausePlayback(): void {
    if (!playbackActive) {
      return;
    }
    setPlaybackPaused(true);
    playbackLastTickMsRef.current = null;
  }

  function stepPlaybackFrame(direction: 1 | -1): void {
    if (!playbackFrames || playbackFrames.length === 0) {
      return;
    }

    setPlaybackPaused(true);
    playbackLastTickMsRef.current = null;

    setPlaybackCursorMs((previousCursorMs) => {
      const currentIndex = getFrameIndexAtCursor(playbackFrames, previousCursorMs);
      if (currentIndex < 0) {
        return previousCursorMs;
      }

      const nextIndex =
        direction > 0
          ? Math.min(playbackFrames.length - 1, currentIndex + 1)
          : Math.max(0, currentIndex - 1);
      const nextFrame = playbackFrames[nextIndex];
      return nextFrame ? nextFrame.capturedAtMs : previousCursorMs;
    });
  }

  const baseShapes = playbackFrame?.baseShapes ?? baseShapesLive;
  const networkTelemetry = playbackFrame?.networkTelemetry ?? networkTelemetryLive;
  const authorityEntities =
    playbackFrame?.authorityEntities ?? authorityEntitiesLive;
  const transferManifest =
    playbackFrame?.transferManifest ?? transferManifestLive;
  const shardPlacement = playbackFrame?.shardPlacement ?? shardPlacementLive;

  const {
    combinedShapes,
    hoveredShardEdgeLabels,
    networkEdgeCount,
    networkNodeIds,
    networkNodeIdSet,
    shardHoverBoundsById,
    shardTelemetryById,
  } = useMapDerivedData({
    baseShapes,
    networkTelemetry,
    authorityEntities,
    authorityLinkMode,
    transferManifest,
    showAuthorityEntities,
    showGnsConnections,
    hoveredShardId,
  });

  const { clearHoveredShard, handleMapPointerWorld } = useShardHoverState({
    hoveredShardId,
    networkNodeIdSet,
    setHoveredShardAnchor,
    setHoveredShardId,
    shardHoverBoundsById,
    showGnsConnections,
    showShardHoverDetails,
  });

  useEffect(() => {
    if (!showShardHoverDetails) {
      rendererRef.current?.draw();
    }
  }, [showShardHoverDetails]);

  useEffect(() => {
    rendererRef.current?.setHoverEdgeLabels(
      showShardHoverDetails && showGnsConnections ? hoveredShardEdgeLabels : []
    );
  }, [hoveredShardEdgeLabels, showGnsConnections, showShardHoverDetails]);

  useEffect(() => {
    const container = containerRef.current;
    if (!container || rendererRef.current) {
      return;
    }

    rendererRef.current = createMapRenderer({
      container,
      shapes: [],
      viewMode: '2d',
      projectionMode: 'orthographic',
      interactionSensitivity: DEFAULT_INTERACTION_SENSITIVITY,
      onPointerWorldPosition: handleMapPointerWorld,
    });

    return () => {
      rendererRef.current?.destroy();
      rendererRef.current = null;
    };
  }, [handleMapPointerWorld]);

  useEffect(() => {
    rendererRef.current?.setShapes(combinedShapes);
  }, [combinedShapes]);

  useEffect(() => {
    rendererRef.current?.setViewMode(viewMode);
  }, [viewMode]);

  useEffect(() => {
    rendererRef.current?.setProjectionMode(projectionMode);
  }, [projectionMode]);

  useEffect(() => {
    rendererRef.current?.setInteractionSensitivity(interactionSensitivity);
  }, [interactionSensitivity]);

  useEffect(() => {
    const hoveredEntity =
      selectedEntities.find(
        (entity) => entity.entityId === hoveredSelectedEntityId
      ) ?? null;
    const inspectedEntity =
      selectedEntities.find((entity) => entity.entityId === activeEntityId) ?? null;
    rendererRef.current?.setEntityFocusOverlay({
      enabled: selectedEntities.length > 0,
      selectedPoints: selectedEntities.map((entity) => ({
        x: entity.x,
        y: entity.y,
      })),
      inspectedPoint: inspectedEntity
        ? {
            x: inspectedEntity.x,
            y: inspectedEntity.y,
          }
        : null,
      hoveredPoint: hoveredEntity
        ? {
            x: hoveredEntity.x,
            y: hoveredEntity.y,
          }
        : null,
    });
  }, [activeEntityId, hoveredSelectedEntityId, selectedEntities]);

  const hoveredTelemetry = hoveredShardId
    ? shardTelemetryById.get(hoveredShardId)
    : undefined;
  const shardWorkerNodeById = useMemo(() => {
    const out = new Map<string, string>();
    for (const row of shardPlacement) {
      const shardId = String(row.shardId || '').trim();
      const nodeName = String(row.nodeName || '').trim();
      if (!shardId || !nodeName || out.has(shardId)) {
        continue;
      }
      out.set(shardId, nodeName);
    }
    return out;
  }, [shardPlacement]);
  const hoveredShardWorkerNode = hoveredShardId
    ? shardWorkerNodeById.get(String(hoveredShardId).trim()) ?? null
    : null;

  return (
    <div
      style={{
        width: '100%',
        height: '100%',
        minHeight: 0,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
        position: 'relative',
      }}
    >
      <div
        ref={containerRef}
        style={{
          width: '100%',
          flex: '1 1 auto',
          minHeight: 0,
          position: 'relative',
          overflow: 'hidden',
          touchAction: 'none',
          border: '1px solid #ccc',
          boxSizing: 'border-box',
        }}
        onPointerDownCapture={onPointerDownCapture}
        onPointerMoveCapture={onPointerMoveCapture}
        onPointerUpCapture={onPointerUpCapture}
        onPointerCancelCapture={onPointerCancelCapture}
        onMouseLeave={clearHoveredShard}
      >
        {showShardHoverDetails && selectedEntities.length === 0 && (
          <ShardHoverTooltip
            hoveredShardId={hoveredShardId}
            hoveredShardAnchor={hoveredShardAnchor}
            downloadBytesPerSec={hoveredTelemetry?.downloadKbps ?? 0}
            uploadBytesPerSec={hoveredTelemetry?.uploadKbps ?? 0}
            outgoingConnectionCount={hoveredShardEdgeLabels.length}
            workerNodeName={hoveredShardWorkerNode}
          />
        )}

        {selectedEntities.length === 0 && (
          <div
            style={{
              position: 'absolute',
              left: 10,
              bottom: 10,
              borderRadius: 6,
              border: '1px solid rgba(148, 163, 184, 0.35)',
              background: 'rgba(2, 6, 23, 0.78)',
              color: '#cbd5e1',
              fontSize: 11,
              padding: '4px 6px',
              pointerEvents: 'none',
            }}
          >
            hold Ctrl + drag to select entities
          </div>
        )}

        {selectionRect && (
          <div
            style={{
              position: 'absolute',
              left: selectionRect.left,
              top: selectionRect.top,
              width: selectionRect.width,
              height: selectionRect.height,
              border: '1px dashed rgba(96, 165, 250, 0.98)',
              background: 'rgba(59, 130, 246, 0.2)',
              pointerEvents: 'none',
              zIndex: 2,
            }}
          />
        )}

        <MapPlaybackBar
          visible={playbackActive}
          startMs={playbackStartMs}
          endMs={playbackEndMs}
          cursorMs={playbackCursorMs}
          paused={playbackPaused}
          direction={playbackDirection}
          onPlayForward={playForward}
          onPlayReverse={playReverse}
          onPause={pausePlayback}
          onStepForward={() => stepPlaybackFrame(1)}
          onStepReverse={() => stepPlaybackFrame(-1)}
          onSeek={seekPlayback}
          onResumeLive={resumeLive}
        />
      </div>

      <MapHud
        showGnsConnections={showGnsConnections}
        showAuthorityEntities={showAuthorityEntities}
        authorityLinkMode={authorityLinkMode}
        showShardHoverDetails={showShardHoverDetails}
        onToggleGnsConnections={() => setShowGnsConnections((value) => !value)}
        onToggleAuthorityEntities={() => setShowAuthorityEntities((value) => !value)}
        onSetAuthorityLinkMode={setAuthorityLinkMode}
        onToggleShardHoverDetails={() => setShowShardHoverDetails((value) => !value)}
        entityCount={authorityEntities.length}
        shardCount={networkNodeIds.length}
        networkEdgeCount={networkEdgeCount}
        claimedEntityCount={authorityEntities.length}
        viewMode={viewMode}
        projectionMode={projectionMode}
        onSetViewMode={setViewMode}
        onSetProjectionMode={setProjectionMode}
        interactionSensitivity={interactionSensitivity}
        onSetInteractionSensitivity={setInteractionSensitivity}
        playbackActive={playbackActive}
        onTakeSnapshot={takeSnapshot}
        onResumeLive={resumeLive}
        pollIntervalMs={pollIntervalMs}
        minPollIntervalMs={MIN_POLL_INTERVAL_MS}
        maxPollIntervalMs={MAX_POLL_INTERVAL_MS}
        onSetPollIntervalMs={setPollIntervalMs}
      />

      <EntityInspectorPanel
        selectedEntities={selectedEntities}
        activeEntityId={activeEntityId}
        hoveredEntityId={hoveredSelectedEntityId}
        detailsState={entityDetails || EMPTY_DETAILS_STATE}
        playbackMode={playbackActive}
        pollIntervalMs={entityDetailPollIntervalMs}
        minPollIntervalMs={MIN_ENTITY_DETAIL_POLL_INTERVAL_MS}
        maxPollIntervalMs={MAX_ENTITY_DETAIL_POLL_INTERVAL_MS}
        pollDisabledAtMs={ENTITY_DETAIL_POLL_DISABLED_AT_MS}
        onSetPollIntervalMs={setEntityDetailPollIntervalMs}
        onSelectEntity={(entityId) => {
          setActiveEntityId(activeEntityId === entityId ? null : entityId);
        }}
        onHoverEntity={(entityId) => {
          setHoveredEntityId(entityId);
        }}
        onClearSelection={() => {
          clearSelection();
        }}
      />
    </div>
  );
}
