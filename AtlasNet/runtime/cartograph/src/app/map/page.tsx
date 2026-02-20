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
  type MapViewPreset,
  type MapViewMode,
} from '../lib/mapRenderer';
import { MapHud } from './components/MapHud';
import { ShardHoverTooltip } from './components/ShardHoverTooltip';

const DEFAULT_POLL_INTERVAL_MS = 50;
const MIN_POLL_INTERVAL_MS = 50;
const MAX_POLL_INTERVAL_MS = 1000;
const DEFAULT_INTERACTION_SENSITIVITY = 1;
const MIN_INTERACTION_SENSITIVITY = 0.1;
const MAX_INTERACTION_SENSITIVITY = 6;

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

  function snapToPreset(preset: MapViewPreset) {
    setViewMode('3d');
    rendererRef.current?.setViewPreset(preset);
  }

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
        onMouseLeave={clearHoveredShard}
      >
        {showShardHoverDetails && (
          <ShardHoverTooltip
            hoveredShardId={hoveredShardId}
            hoveredShardAnchor={hoveredShardAnchor}
            downloadBytesPerSec={hoveredTelemetry?.downloadKbps ?? 0}
            uploadBytesPerSec={hoveredTelemetry?.uploadKbps ?? 0}
            outgoingConnectionCount={hoveredShardEdgeLabels.length}
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
        onSetViewPreset={snapToPreset}
        interactionSensitivity={interactionSensitivity}
        minInteractionSensitivity={MIN_INTERACTION_SENSITIVITY}
        maxInteractionSensitivity={MAX_INTERACTION_SENSITIVITY}
        onSetInteractionSensitivity={setInteractionSensitivity}
        pollIntervalMs={pollIntervalMs}
        minPollIntervalMs={MIN_POLL_INTERVAL_MS}
        maxPollIntervalMs={MAX_POLL_INTERVAL_MS}
        onSetPollIntervalMs={setPollIntervalMs}
      />
    </div>
  );
}
