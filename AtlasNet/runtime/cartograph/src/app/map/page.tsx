'use client';

import { useEffect, useRef, useState } from 'react';
import { useMapDerivedData } from '../lib/hooks/useMapDerivedData';
import { useShardHoverState } from '../lib/hooks/useShardHoverState';
import {
  useAuthorityEntities,
  useHeuristicShapes,
  useNetworkTelemetry,
} from '../lib/hooks/useTelemetryFeeds';
import {
  createMapRenderer,
  type MapProjectionMode,
  type MapViewMode,
} from '../lib/mapRenderer';
import { EntityInspectorPanel } from './entityInspector/EntityInspectorPanel';
import { useCtrlDragEntitySelection } from './entityInspector/useCtrlDragEntitySelection';
import { MapHud } from './components/MapHud';
import { ShardHoverTooltip } from './components/ShardHoverTooltip';

const DEFAULT_POLL_INTERVAL_MS = 50;
const MIN_POLL_INTERVAL_MS = 50;
const MAX_POLL_INTERVAL_MS = 1000;
const DEFAULT_ENTITY_DETAIL_POLL_INTERVAL_MS = 1000;
const MIN_ENTITY_DETAIL_POLL_INTERVAL_MS = 250;
const MAX_ENTITY_DETAIL_POLL_INTERVAL_MS = 5000;
const ENTITY_DETAIL_POLL_DISABLED_AT_MS = MAX_ENTITY_DETAIL_POLL_INTERVAL_MS;
const DEFAULT_INTERACTION_SENSITIVITY = 1;

export default function MapPage() {
  const containerRef = useRef<HTMLDivElement>(null);
  const rendererRef = useRef<ReturnType<typeof createMapRenderer> | null>(null);
  const [showGnsConnections, setShowGnsConnections] = useState(true);
  const [showAuthorityEntities, setShowAuthorityEntities] = useState(true);
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

  const baseShapes = useHeuristicShapes({
    intervalMs: pollIntervalMs,
    resetOnException: true,
    resetOnHttpError: false,
  });
  const networkTelemetry = useNetworkTelemetry({
    intervalMs: pollIntervalMs,
    resetOnException: true,
    resetOnHttpError: false,
  });
  const authorityEntities = useAuthorityEntities({
    intervalMs: pollIntervalMs,
    resetOnException: true,
    resetOnHttpError: false,
  });
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
    entities: authorityEntities,
  });

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

      </div>

      <MapHud
        showGnsConnections={showGnsConnections}
        showAuthorityEntities={showAuthorityEntities}
        showShardHoverDetails={showShardHoverDetails}
        onToggleGnsConnections={() => setShowGnsConnections((value) => !value)}
        onToggleAuthorityEntities={() => setShowAuthorityEntities((value) => !value)}
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
        pollIntervalMs={pollIntervalMs}
        minPollIntervalMs={MIN_POLL_INTERVAL_MS}
        maxPollIntervalMs={MAX_POLL_INTERVAL_MS}
        onSetPollIntervalMs={setPollIntervalMs}
      />

      <EntityInspectorPanel
        selectedEntities={selectedEntities}
        activeEntityId={activeEntityId}
        hoveredEntityId={hoveredSelectedEntityId}
        pollIntervalMs={entityDetailPollIntervalMs}
        minPollIntervalMs={MIN_ENTITY_DETAIL_POLL_INTERVAL_MS}
        maxPollIntervalMs={MAX_ENTITY_DETAIL_POLL_INTERVAL_MS}
        pollDisabledAtMs={ENTITY_DETAIL_POLL_DISABLED_AT_MS}
        onSetPollIntervalMs={setEntityDetailPollIntervalMs}
        onSelectEntity={(entityId) =>
          setActiveEntityId(activeEntityId === entityId ? null : entityId)
        }
        onHoverEntity={setHoveredEntityId}
        onClearSelection={clearSelection}
      />
    </div>
  );
}