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
} from '../lib/cartographTypes';
import { useMapDerivedData } from '../lib/hooks/useMapDerivedData';
import { useShardHoverState } from '../lib/hooks/useShardHoverState';
import {
  useAuthorityEntities,
  useHeuristicShapes,
  useNetworkTelemetry,
  useShardPlacement,
  useTransferStateQueue,
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
const TRANSFER_STATE_TIMESTAMP_SLOT_MS = 10;
const DEFAULT_PLAYBACK_TIME_TICK_MS = 10;
const MIN_PLAYBACK_TIME_TICK_MS = 10;
const MAX_PLAYBACK_TIME_TICK_MS = 100;
const PLAYBACK_TIME_TICK_STEP_MS = 10;
const TRANSFER_EVENT_MAX_AGE_MS = 1;

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

interface ActiveTransferQueueEvent {
  event: TransferStateQueueTelemetry;
}

interface LatestTransferEventByEntity {
  event: TransferStateQueueTelemetry;
  sourceTimestampMs: number;
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

function normalizeTimestampMs(value: unknown, fallbackMs: number): number {
  const timestampMsRaw = Number(value);
  if (!Number.isFinite(timestampMsRaw) || timestampMsRaw <= 0) {
    return fallbackMs;
  }
  return Math.floor(timestampMsRaw);
}

function snapToNearestTickMs(valueMs: number, tickMs: number): number {
  const safeTickMs = Math.max(1, Math.floor(tickMs));
  return Math.round(valueMs / safeTickMs) * safeTickMs;
}

function resolveTransferManifestAtCursor(
  events: TransferStateQueueTelemetry[],
  cursorMs: number,
  holdMs: number
): TransferManifestTelemetry[] {
  if (!Array.isArray(events) || events.length === 0) {
    return [];
  }

  const minTimestampMs = cursorMs - Math.max(0, holdMs);
  const latestByEntity = new Map<string, TransferStateQueueTelemetry>();

  for (const event of events) {
    if (!event) {
      continue;
    }

    const timestampMs = normalizeTimestampMs(event.timestampMs, 0);
    if (timestampMs <= 0) {
      continue;
    }
    if (timestampMs < minTimestampMs || timestampMs > cursorMs) {
      continue;
    }

    const entityIds = Array.isArray(event.entityIds) ? event.entityIds : [];
    for (const rawEntityId of entityIds) {
      const entityId = String(rawEntityId || '').trim();
      if (!entityId) {
        continue;
      }

      latestByEntity.set(entityId, {
        ...event,
        entityIds: [entityId],
        timestampMs,
      });
    }
  }

  const rows: TransferManifestTelemetry[] = [];
  for (const [entityId, event] of latestByEntity.entries()) {
    rows.push({
      transferId: `${event.transferId}:${entityId}:${event.timestampMs}`,
      fromId: event.fromId,
      toId: event.toId,
      stage: event.stage,
      state: event.state,
      entityIds: [entityId],
      timestampMs: event.timestampMs,
    });
  }
  return rows;
}

export default function MapPage() {
  const containerRef = useRef<HTMLDivElement>(null);
  const rendererRef = useRef<ReturnType<typeof createMapRenderer> | null>(null);
  const historyRef = useRef<PlaybackFrame[]>([]);
  const transferLastTimestampByEntityRef = useRef<Map<string, number>>(new Map());
  const transferTimelineRef = useRef<TransferStateQueueTelemetry[]>([]);
  const transferPollStartedAtMsRef = useRef<number>(0);
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
    const timeline = transferTimelineRef.current;
    const lastTimestampByEntity = transferLastTimestampByEntityRef.current;
    const latestEventByEntity = new Map<string, LatestTransferEventByEntity>();
    const latestByEntity: Record<string, ActiveTransferQueueEvent> = {};

    for (const event of events) {
      const baseTimestampMs = normalizeTimestampMs(event?.timestampMs, nowMs);
      const snappedTimestampMs = snapToNearestTickMs(
        baseTimestampMs,
        TRANSFER_STATE_TIMESTAMP_SLOT_MS
      );
      const entityIds = Array.isArray(event?.entityIds) ? event.entityIds : [];
      for (const rawEntityId of entityIds) {
        const entityId = String(rawEntityId || '').trim();
        if (!entityId) {
          continue;
        }

        const previousTimestampMs = lastTimestampByEntity.get(entityId) ?? -1;
        let adjustedTimestampMs = snappedTimestampMs;
        while (adjustedTimestampMs <= previousTimestampMs) {
          adjustedTimestampMs += TRANSFER_STATE_TIMESTAMP_SLOT_MS;
        }
        lastTimestampByEntity.set(entityId, adjustedTimestampMs);

        const perEntityEvent: TransferStateQueueTelemetry = {
          ...event,
          entityIds: [entityId],
          timestampMs: adjustedTimestampMs,
        };
        timeline.push(perEntityEvent);
        const currentLatest = latestEventByEntity.get(entityId);
        if (
          !currentLatest ||
          baseTimestampMs > currentLatest.sourceTimestampMs ||
          (baseTimestampMs === currentLatest.sourceTimestampMs &&
            perEntityEvent.timestampMs >= currentLatest.event.timestampMs)
        ) {
          latestEventByEntity.set(entityId, {
            event: perEntityEvent,
            sourceTimestampMs: baseTimestampMs,
          });
        }
      }
    }

    const cutoffMs = nowMs - SNAPSHOT_WINDOW_MS;
    while (timeline.length > 0 && timeline[0].timestampMs < cutoffMs) {
      timeline.shift();
    }

    for (const [entityId, latest] of latestEventByEntity.entries()) {
      const ageMs = Math.max(0, pollStartedAtMs - latest.sourceTimestampMs);
      if (ageMs > TRANSFER_EVENT_MAX_AGE_MS) {
        continue;
      }
      latestByEntity[entityId] = { event: latest.event };
    }

    setActiveTransferQueueByEntity(latestByEntity);
  }, [transferStateQueueLive]);

  const transferManifestLiveResolved = useMemo(() => {
    const rows: TransferManifestTelemetry[] = [];
    for (const [entityId, active] of Object.entries(activeTransferQueueByEntity)) {
      const event = active.event;
      rows.push({
        transferId: `${event.transferId}:${entityId}:${event.timestampMs}`,
        fromId: event.fromId,
        toId: event.toId,
        stage: event.stage,
        state: event.state,
        entityIds: [entityId],
        timestampMs: event.timestampMs,
      });
    }
    return rows;
  }, [activeTransferQueueByEntity]);

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
      transferManifest: transferManifestLiveResolved,
      shardPlacement: shardPlacementLive,
      entityDetailsByEntityId: snapshotDetailsByEntityId(
        liveEntityDetailsCacheRef.current
      ),
    };
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
    const timeline = transferTimelineRef.current;
    let transferEvents = timeline.filter(
      (event) =>
        event.timestampMs >= endMs - SNAPSHOT_WINDOW_MS && event.timestampMs <= endMs
    );
    if (transferEvents.length === 0 && timeline.length > 0) {
      const latestEventTimestampMs = timeline[timeline.length - 1].timestampMs;
      transferEvents = timeline.filter(
        (event) => event.timestampMs >= latestEventTimestampMs - SNAPSHOT_WINDOW_MS
      );
    }
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

  function stepPlaybackFrame(direction: 1 | -1): void {
    if (!playbackFrames || playbackFrames.length === 0) {
      return;
    }

    setPlaybackPaused(true);

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
    combinedShapes,
    hoveredShardEdgeLabels,
    networkEdgeCount,
    networkNodeIds,
    networkNodeIdSet,
    shardHoverBoundsById,
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
  });

  const { clearHoveredShard, handleMapPointerWorld } = useShardHoverState({
    hoveredShardId,
    networkNodeIdSet,
    setHoveredShardAnchor,
    setHoveredShardId,
    shardHoverBoundsById,
    shardHoverPolygonsById: shardPolygonsById,
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
        onContextMenuCapture={onContextMenuCapture}
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
          onStepForward={() => stepPlaybackFrame(1)}
          onStepReverse={() => stepPlaybackFrame(-1)}
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
