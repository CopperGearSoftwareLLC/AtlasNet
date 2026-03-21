'use client';

import { useEffect, useMemo, useRef, useState } from 'react';
import type {
  AuthorityEntityTelemetry,
  AuthorityLinkMode,
  ShapeJS,
  ShardPlacementTelemetry,
  ShardTelemetry,
  TransferManifestTelemetry,
  TransferStateQueueTelemetry,
} from '../shared/cartographTypes';
import { useMapDerivedData } from './core/useMapDerivedData';
import { useMapNodeFilterData } from './core/useMapNodeFilterData';
import { useShardHoverState } from './mouse-hover/useShardHoverState';
import {
  useAuthorityEntities,
  useHeuristicShapes,
  useNetworkTelemetry,
  useShardPlacement,
  useTransferStateQueue,
} from '../shared/useTelemetryPolling';
import {
  createMapRenderer,
  type MapProjectionMode,
  type MapViewMode,
} from './core/mapRenderer';
import { EntityInspectorPanel } from './entity-inspector/EntityInspectorPanel';
import {
  useEntityInspectorLookup,
  type EntityInspectorLookupState,
} from './entity-inspector/useEntityInspectorLookup';
import { useCtrlDragEntitySelection } from './entity-inspector/useCtrlDragEntitySelection';
import { MapHud } from './MapHud';
import { NodeFilterPanel } from './NodeFilterPanel';
import { MapPlaybackBar } from './playback/MapPlaybackBar';
import { ShardHoverTooltip } from './mouse-hover/ShardHoverTooltip';
import {
  buildLiveTransferManifest,
  ingestTransferQueueEvents,
  resolveTransferManifestAtCursor,
  selectSnapshotTransferEvents,
  snapToNearestTickMs,
  type ActiveTransferQueueEvent,
} from './playback/transferPlaybackTimeline';
import {
  normalizeShardId,
} from './core/mapData';

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
const TRANSFER_STATE_TIMESTAMP_SLOT_MS = 10;
const DEFAULT_PLAYBACK_TIME_TICK_MS = 10;
const MIN_PLAYBACK_TIME_TICK_MS = 10;
const MAX_PLAYBACK_TIME_TICK_MS = 100;
const PLAYBACK_TIME_TICK_STEP_MS = 10;

const EMPTY_DETAILS_STATE: EntityInspectorLookupState = {
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
  entityDetailsByEntityId: Record<string, EntityInspectorLookupState>;
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
  cache: Map<string, EntityInspectorLookupState>
): Record<string, EntityInspectorLookupState> {
  const out: Record<string, EntityInspectorLookupState> = {};
  for (const [entityId, state] of cache.entries()) {
    out[entityId] = state;
  }
  return out;
}

export default function MapPage() {
  const containerRef = useRef<HTMLDivElement>(null);
  const rendererRef = useRef<ReturnType<typeof createMapRenderer> | null>(null);
  const historyRef = useRef<PlaybackFrame[]>([]);
  const lastSnapshotCaptureAtMsRef = useRef(0);
  const transferLastTimestampByEntityRef = useRef<Map<string, number>>(new Map());
  const transferTimelineRef = useRef<TransferStateQueueTelemetry[]>([]);
  const transferPollStartedAtMsRef = useRef<number>(0);
  const [showGnsConnections, setShowGnsConnections] = useState(true);
  const [showAuthorityEntities, setShowAuthorityEntities] = useState(true);
  const [authorityLinkMode, setAuthorityLinkMode] =
    useState<AuthorityLinkMode>('owner');
  const [showShardHoverDetails] = useState(true);
  const [showEntityOwnershipHover, setShowEntityOwnershipHover] = useState(true);
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
  const [playbackTransferEvents, setPlaybackTransferEvents] = useState<
    TransferStateQueueTelemetry[] | null
  >(null);
  const [playbackCursorMs, setPlaybackCursorMs] = useState(0);
  const [playbackTimeTickMs, setPlaybackTimeTickMs] = useState(
    DEFAULT_PLAYBACK_TIME_TICK_MS
  );
  const [playbackPaused, setPlaybackPaused] = useState(true);
  const [playbackDirection, setPlaybackDirection] = useState<1 | -1>(1);
  const [activeTransferQueueByEntity, setActiveTransferQueueByEntity] = useState<
    Record<string, ActiveTransferQueueEvent>
  >({});
  const [showNodeFilterPanel, setShowNodeFilterPanel] = useState(false);
  const [hiddenWorkerNodeNames, setHiddenWorkerNodeNames] = useState<string[]>([]);

  const telemetryPollIntervalMs =
    pollIntervalMs >= POLL_DISABLED_AT_MS ? 0 : pollIntervalMs;
  const transferEventMaxAgeMs =
    telemetryPollIntervalMs > 0
      ? telemetryPollIntervalMs
      : Math.max(MIN_POLL_INTERVAL_MS, pollIntervalMs);
  const baseShapesLive = useHeuristicShapes({
    intervalMs: telemetryPollIntervalMs,
    resetOnException: true,
    resetOnHttpError: false,
  });
  const networkTelemetryLive = useNetworkTelemetry({
    intervalMs: telemetryPollIntervalMs,
    includeLiveIds: true,
    resetOnException: true,
    resetOnHttpError: false,
  });
  const authorityEntitiesLive = useAuthorityEntities({
    intervalMs: telemetryPollIntervalMs,
    resetOnException: true,
    resetOnHttpError: false,
  });
  const transferStateQueueLive = useTransferStateQueue({
    intervalMs: telemetryPollIntervalMs,
    resetOnException: true,
    resetOnHttpError: false,
    onPollResult: (meta) => {
      if (!meta.succeeded) {
        return;
      }
      transferPollStartedAtMsRef.current = Math.floor(meta.pollStartedAtMs);
    },
  });
  const shardPlacementLive = useShardPlacement({
    intervalMs:
      telemetryPollIntervalMs <= 0 ? 0 : Math.max(1000, telemetryPollIntervalMs),
    enabled: true,
    resetOnException: false,
    resetOnHttpError: false,
  });

  useEffect(() => {
    const events = Array.isArray(transferStateQueueLive) ? transferStateQueueLive : [];
    const nowMs = Date.now();
    const pollStartedAtMs =
      transferPollStartedAtMsRef.current > 0
        ? transferPollStartedAtMsRef.current
        : nowMs;
    const ingested = ingestTransferQueueEvents({
      events,
      nowMs,
      pollStartedAtMs,
      timeline: transferTimelineRef.current,
      lastTimestampByEntity: transferLastTimestampByEntityRef.current,
      snapshotWindowMs: SNAPSHOT_WINDOW_MS,
      timestampSlotMs: TRANSFER_STATE_TIMESTAMP_SLOT_MS,
      maxEventAgeMs: transferEventMaxAgeMs,
    });

    transferTimelineRef.current = ingested.timeline;
    transferLastTimestampByEntityRef.current = ingested.lastTimestampByEntity;
    setActiveTransferQueueByEntity(ingested.activeByEntity);
  }, [transferEventMaxAgeMs, transferStateQueueLive]);

  const transferManifestLiveResolved = useMemo(
    () => buildLiveTransferManifest(activeTransferQueueByEntity),
    [activeTransferQueueByEntity]
  );

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
    onContextMenuCapture,
    setActiveEntityId,
    setHoveredEntityId,
  } = useCtrlDragEntitySelection({
    containerRef,
    rendererRef,
    entities: authorityEntitiesForSelection,
    viewMode,
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
  const liveEntityDetails = useEntityInspectorLookup(
    activeEntityLive,
    entityDetailPollIntervalMs
  );
  const playbackLookupDetails = useEntityInspectorLookup(
    playbackActive ? activeSelectedEntity : null,
    0
  );
  const liveEntityDetailsCacheRef = useRef<Map<string, EntityInspectorLookupState>>(
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

  useEffect(() => {
    const nowMs = Date.now();
    if (nowMs - lastSnapshotCaptureAtMsRef.current < SNAPSHOT_RECORD_SPACING_MS) {
      return;
    }
    lastSnapshotCaptureAtMsRef.current = nowMs;

    const frame: PlaybackFrame = {
      capturedAtMs: nowMs,
      baseShapes: baseShapesLive,
      networkTelemetry: networkTelemetryLive,
      authorityEntities: authorityEntitiesLive,
      transferManifest: transferManifestLiveResolved,
      shardPlacement: shardPlacementLive,
      entityDetailsByEntityId: snapshotDetailsByEntityId(
        liveEntityDetailsCacheRef.current
      ),
    };

    const frames = historyRef.current;
    frames.push(frame);

    const cutoffMs = nowMs - SNAPSHOT_WINDOW_MS;
    while (frames.length > 0 && frames[0].capturedAtMs < cutoffMs) {
      frames.shift();
    }
  }, [
    baseShapesLive,
    networkTelemetryLive,
    authorityEntitiesLive,
    transferManifestLiveResolved,
    shardPlacementLive,
    liveEntityDetails,
    activeEntityLive,
  ]);

  useEffect(() => {
    if (!playbackActive || playbackPaused) {
      return;
    }

    const intervalId = setInterval(() => {
      setPlaybackCursorMs((prev) => {
        const next = prev + playbackDirection * playbackTimeTickMs;
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
    playbackTimeTickMs,
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
    const transferEvents = selectSnapshotTransferEvents({
      timeline: transferTimelineRef.current,
      endMs,
      snapshotWindowMs: SNAPSHOT_WINDOW_MS,
    });
    setPlaybackFrames(snapshot);
    setPlaybackTransferEvents(transferEvents);
    setPlaybackCursorMs(endMs);
    setPlaybackDirection(1);
    setPlaybackPaused(true);
  }

  function resumeLive(): void {
    setPlaybackFrames(null);
    setPlaybackTransferEvents(null);
    setPlaybackPaused(true);
  }

  function seekPlayback(nextCursorMs: number): void {
    if (!playbackActive) {
      return;
    }
    const minMs = playbackStartMs;
    const maxMs = playbackEndMs;
    const clamped = Math.max(minMs, Math.min(maxMs, nextCursorMs));
    const snapped = snapToNearestTickMs(clamped, playbackTimeTickMs);
    setPlaybackCursorMs(Math.max(minMs, Math.min(maxMs, snapped)));
  }

  function playForward(): void {
    if (!playbackActive) {
      return;
    }
    setPlaybackDirection(1);
    setPlaybackPaused(false);
  }

  function playReverse(): void {
    if (!playbackActive) {
      return;
    }
    setPlaybackDirection(-1);
    setPlaybackPaused(false);
  }

  function pausePlayback(): void {
    if (!playbackActive) {
      return;
    }
    setPlaybackPaused(true);
  }

  function stepPlaybackTick(direction: 1 | -1): void {
    if (!playbackActive) {
      return;
    }

    setPlaybackPaused(true);

    setPlaybackCursorMs((previousCursorMs) => {
      const snappedCursorMs = snapToNearestTickMs(previousCursorMs, playbackTimeTickMs);
      const nextCursorMs = snappedCursorMs + direction * playbackTimeTickMs;
      return Math.max(playbackStartMs, Math.min(playbackEndMs, nextCursorMs));
    });
  }

  function updatePlaybackTimeTickMs(nextTickMs: number): void {
    if (!Number.isFinite(nextTickMs)) {
      return;
    }
    const roundedToStepMs =
      Math.round(nextTickMs / PLAYBACK_TIME_TICK_STEP_MS) * PLAYBACK_TIME_TICK_STEP_MS;
    const clampedTickMs = Math.max(
      MIN_PLAYBACK_TIME_TICK_MS,
      Math.min(MAX_PLAYBACK_TIME_TICK_MS, roundedToStepMs)
    );
    setPlaybackTimeTickMs(clampedTickMs);
    if (!playbackActive) {
      return;
    }
    setPlaybackCursorMs((previousCursorMs) => {
      const snapped = snapToNearestTickMs(previousCursorMs, clampedTickMs);
      return Math.max(playbackStartMs, Math.min(playbackEndMs, snapped));
    });
  }

  const baseShapes = playbackFrame?.baseShapes ?? baseShapesLive;
  const networkTelemetry = playbackFrame?.networkTelemetry ?? networkTelemetryLive;
  const authorityEntities =
    playbackFrame?.authorityEntities ?? authorityEntitiesLive;
  const playbackTransferManifest = useMemo(
    () =>
      playbackActive && playbackTransferEvents
        ? resolveTransferManifestAtCursor(
            playbackTransferEvents,
            playbackCursorMs,
            playbackTimeTickMs
          )
        : [],
    [playbackActive, playbackCursorMs, playbackTimeTickMs, playbackTransferEvents]
  );
  const transferManifest = playbackActive
    ? playbackTransferManifest
    : transferManifestLiveResolved;
  const shardPlacement = playbackFrame?.shardPlacement ?? shardPlacementLive;
  const {
    clusterNodeNames,
    clusterNodes,
    filteredShardIdSet,
    hiddenWorkerNodeNameSet,
    selectedWorkerNodeNameSet,
    shardWorkerNodeById,
    visibleShardCount,
  } = useMapNodeFilterData({
    baseShapes,
    networkTelemetry,
    shardPlacement,
    hiddenWorkerNodeNames,
  });

  const {
    combinedShapes,
    hoveredShardEdgeLabels,
    networkEdgeCount,
    networkNodeIds,
    networkNodeIdSet,
    shardHoverBoundsById,
    shardHoverRegionsById,
    shardPolygonsById,
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
    showEntityOwnershipHover,
    filteredShardIdSet,
  });

  useEffect(() => {
    if (clusterNodeNames.length === 0) {
      setHiddenWorkerNodeNames((previous) =>
        previous.length === 0 ? previous : []
      );
      return;
    }

    setHiddenWorkerNodeNames((previous) =>
      previous.filter((nodeName) => clusterNodeNames.includes(nodeName))
    );
  }, [clusterNodeNames]);

  const { clearHoveredShard, handleMapPointerWorld } = useShardHoverState({
    hoveredShardId,
    networkNodeIdSet,
    setHoveredShardAnchor,
    setHoveredShardId,
    shardHoverBoundsById,
    shardHoverRegionsById,
    shardHoverPolygonsById: shardPolygonsById,
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
        isClient: entity.isClient,
      })),
      inspectedPoint: inspectedEntity
        ? {
            x: inspectedEntity.x,
            y: inspectedEntity.y,
            isClient: inspectedEntity.isClient,
          }
        : null,
      hoveredPoint: hoveredEntity
        ? {
            x: hoveredEntity.x,
            y: hoveredEntity.y,
            isClient: hoveredEntity.isClient,
          }
        : null,
    });
  }, [activeEntityId, hoveredSelectedEntityId, selectedEntities]);

  const hoveredTelemetry = hoveredShardId
    ? shardTelemetryById.get(hoveredShardId)
    : undefined;
  const hoveredShardWorkerNode = hoveredShardId
    ? shardWorkerNodeById.get(normalizeShardId(hoveredShardId)) ?? null
    : null;

  function toggleWorkerNode(nodeName: string): void {
    setHiddenWorkerNodeNames((previous) => {
      const hidden = new Set(previous);
      if (hidden.has(nodeName)) {
        hidden.delete(nodeName);
      } else {
        hidden.add(nodeName);
      }
      return clusterNodeNames.filter((name) => hidden.has(name));
    });
  }

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
        onContextMenuCapture={onContextMenuCapture}
        onMouseLeave={clearHoveredShard}
      >
        <NodeFilterPanel
          open={showNodeFilterPanel}
          nodes={clusterNodes}
          selectedNodeCount={selectedWorkerNodeNameSet.size}
          totalShardCount={networkNodeIds.length}
          visibleShardCount={visibleShardCount}
          onToggleOpen={() => setShowNodeFilterPanel((value) => !value)}
          onShowAllNodes={() => setHiddenWorkerNodeNames([])}
          onHideAllNodes={() => setHiddenWorkerNodeNames(clusterNodeNames)}
          onToggleNode={toggleWorkerNode}
          isNodeSelected={(nodeName) => !hiddenWorkerNodeNameSet.has(nodeName)}
        />

        {showShardHoverDetails && selectedEntities.length === 0 && (
          <ShardHoverTooltip
            hoveredShardId={hoveredShardId}
            hoveredShardAnchor={hoveredShardAnchor}
            downloadBytesPerSec={hoveredTelemetry?.downloadKbps ?? 0}
            uploadBytesPerSec={hoveredTelemetry?.uploadKbps ?? 0}
            avgPingMs={hoveredTelemetry?.avgPingMs ?? null}
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
            {viewMode === '2d'
              ? 'right-click + drag to select entities'
              : 'hold Ctrl + drag to select entities'}
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
          onStepForward={() => stepPlaybackTick(1)}
          onStepReverse={() => stepPlaybackTick(-1)}
          timeTickMs={playbackTimeTickMs}
          minTimeTickMs={MIN_PLAYBACK_TIME_TICK_MS}
          maxTimeTickMs={MAX_PLAYBACK_TIME_TICK_MS}
          onTimeTickChange={updatePlaybackTimeTickMs}
          onSeek={seekPlayback}
          onResumeLive={resumeLive}
        />
      </div>

      <MapHud
        showGnsConnections={showGnsConnections}
        showAuthorityEntities={showAuthorityEntities}
        authorityLinkMode={authorityLinkMode}
        showEntityOwnershipHover={showEntityOwnershipHover}
        onToggleGnsConnections={() => setShowGnsConnections((value) => !value)}
        onToggleAuthorityEntities={() => setShowAuthorityEntities((value) => !value)}
        onSetAuthorityLinkMode={setAuthorityLinkMode}
        onToggleEntityOwnershipHover={() =>
          setShowEntityOwnershipHover((value) => !value)
        }
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
