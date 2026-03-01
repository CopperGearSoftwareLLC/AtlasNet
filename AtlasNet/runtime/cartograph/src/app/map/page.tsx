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
const SNAPSHOT_RECORD_SPACING_MS = 100;
const PLAYBACK_STEP_MS = 100;

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
  selectedEntityIds: string[];
  activeEntityId: string | null;
  hoveredEntityId: string | null;
  entityDetails: EntityDatabaseDetailsState;
}

function resolveSelectedEntities(
  selectedIds: string[],
  entities: AuthorityEntityTelemetry[]
): AuthorityEntityTelemetry[] {
  if (selectedIds.length === 0 || entities.length === 0) {
    return [];
  }

  const byId = new Map<string, AuthorityEntityTelemetry>();
  for (const entity of entities) {
    const entityId = String(entity.entityId || '').trim();
    if (!entityId || byId.has(entityId)) {
      continue;
    }
    byId.set(entityId, entity);
  }

  const out: AuthorityEntityTelemetry[] = [];
  for (const selectedId of selectedIds) {
    const entity = byId.get(String(selectedId || '').trim());
    if (entity) {
      out.push(entity);
    }
  }
  return out;
}

function getFrameAtCursor(
  frames: PlaybackFrame[],
  cursorMs: number
): PlaybackFrame | null {
  if (frames.length === 0) {
    return null;
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

  return frames[best] ?? frames[0] ?? null;
}

export default function MapPage() {
  const containerRef = useRef<HTMLDivElement>(null);
  const rendererRef = useRef<ReturnType<typeof createMapRenderer> | null>(null);
  const historyRef = useRef<PlaybackFrame[]>([]);
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
  const {
    selectedEntities: selectedEntitiesLive,
    selectedEntityIds: selectedEntityIdsLive,
    activeEntityId: activeEntityIdLive,
    hoveredEntityId: hoveredSelectedEntityIdLive,
    selectionRect: selectionRectLive,
    clearSelection: clearSelectionLive,
    onPointerDownCapture: onPointerDownCaptureLive,
    onPointerMoveCapture: onPointerMoveCaptureLive,
    onPointerUpCapture: onPointerUpCaptureLive,
    onPointerCancelCapture: onPointerCancelCaptureLive,
    setActiveEntityId: setActiveEntityIdLive,
    setHoveredEntityId: setHoveredEntityIdLive,
  } = useCtrlDragEntitySelection({
    containerRef,
    rendererRef,
    entities: authorityEntitiesLive,
  });

  const activeEntityLive =
    selectedEntitiesLive.find((entity) => entity.entityId === activeEntityIdLive) ??
    null;
  const liveEntityDetails = useEntityDatabaseDetails(
    activeEntityLive,
    playbackFrames ? 0 : entityDetailPollIntervalMs
  );

  useEffect(() => {
    if (playbackFrames) {
      return;
    }

    const nowMs = Date.now();
    const frame: PlaybackFrame = {
      capturedAtMs: nowMs,
      baseShapes: baseShapesLive,
      networkTelemetry: networkTelemetryLive,
      authorityEntities: authorityEntitiesLive,
      transferManifest: transferManifestLive,
      shardPlacement: shardPlacementLive,
      selectedEntityIds: [...selectedEntityIdsLive],
      activeEntityId: activeEntityIdLive,
      hoveredEntityId: hoveredSelectedEntityIdLive,
      entityDetails: liveEntityDetails,
    };

    const frames = historyRef.current;
    const last = frames[frames.length - 1];
    if (last && nowMs - last.capturedAtMs < SNAPSHOT_RECORD_SPACING_MS) {
      frames[frames.length - 1] = frame;
    } else {
      frames.push(frame);
    }

    const cutoffMs = nowMs - SNAPSHOT_WINDOW_MS;
    while (frames.length > 0 && frames[0].capturedAtMs < cutoffMs) {
      frames.shift();
    }
  }, [
    playbackFrames,
    baseShapesLive,
    networkTelemetryLive,
    authorityEntitiesLive,
    transferManifestLive,
    shardPlacementLive,
    selectedEntityIdsLive,
    activeEntityIdLive,
    hoveredSelectedEntityIdLive,
    liveEntityDetails,
  ]);

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

  useEffect(() => {
    if (!playbackActive || playbackPaused) {
      return;
    }

    const intervalId = setInterval(() => {
      setPlaybackCursorMs((prev) => {
        const next = prev + playbackDirection * PLAYBACK_STEP_MS;
        if (playbackDirection < 0) {
          return Math.max(playbackStartMs, next);
        }
        return Math.min(playbackEndMs, next);
      });
    }, PLAYBACK_STEP_MS);

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
  }

  function resumeLive(): void {
    setPlaybackFrames(null);
    setPlaybackPaused(true);
  }

  function seekPlayback(nextCursorMs: number): void {
    if (!playbackActive) {
      return;
    }
    const minMs = playbackStartMs;
    const maxMs = playbackEndMs;
    setPlaybackCursorMs(Math.max(minMs, Math.min(maxMs, nextCursorMs)));
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

  function togglePlaybackPause(): void {
    if (!playbackActive) {
      return;
    }
    setPlaybackPaused((value) => !value);
  }

  const baseShapes = playbackFrame?.baseShapes ?? baseShapesLive;
  const networkTelemetry = playbackFrame?.networkTelemetry ?? networkTelemetryLive;
  const authorityEntities =
    playbackFrame?.authorityEntities ?? authorityEntitiesLive;
  const transferManifest =
    playbackFrame?.transferManifest ?? transferManifestLive;
  const shardPlacement = playbackFrame?.shardPlacement ?? shardPlacementLive;
  const selectedEntities = playbackFrame
    ? resolveSelectedEntities(playbackFrame.selectedEntityIds, authorityEntities)
    : selectedEntitiesLive;
  const activeEntityId = playbackFrame?.activeEntityId ?? activeEntityIdLive;
  const hoveredSelectedEntityId =
    playbackFrame?.hoveredEntityId ?? hoveredSelectedEntityIdLive;
  const selectionRect = playbackFrame ? null : selectionRectLive;
  const entityDetails = playbackFrame?.entityDetails ?? liveEntityDetails;

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
        onPointerDownCapture={playbackActive ? undefined : onPointerDownCaptureLive}
        onPointerMoveCapture={playbackActive ? undefined : onPointerMoveCaptureLive}
        onPointerUpCapture={playbackActive ? undefined : onPointerUpCaptureLive}
        onPointerCancelCapture={playbackActive ? undefined : onPointerCancelCaptureLive}
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

        {!playbackActive && selectedEntities.length === 0 && (
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

        {selectionRect && !playbackActive && (
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
          onTogglePause={togglePlaybackPause}
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
          if (playbackActive) {
            return;
          }
          setActiveEntityIdLive(activeEntityIdLive === entityId ? null : entityId);
        }}
        onHoverEntity={(entityId) => {
          if (playbackActive) {
            return;
          }
          setHoveredEntityIdLive(entityId);
        }}
        onClearSelection={() => {
          if (playbackActive) {
            return;
          }
          clearSelectionLive();
        }}
      />
    </div>
  );
}
