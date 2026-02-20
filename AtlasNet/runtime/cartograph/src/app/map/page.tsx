'use client';

import {
  useCallback,
  useEffect,
  useMemo,
  useRef,
  useState,
  type CSSProperties,
} from 'react';
import type { ShapeJS } from '../lib/cartographTypes';
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

const DEFAULT_POLL_INTERVAL_MS = 200;
const MIN_POLL_INTERVAL_MS = 50;
const MAX_POLL_INTERVAL_MS = 1000;
const DEFAULT_INTERACTION_SENSITIVITY = 1;
const MIN_INTERACTION_SENSITIVITY = 0.1;
const MAX_INTERACTION_SENSITIVITY = 6;
const HUD_ACTIVE_BUTTON_BG = 'rgba(56, 189, 248, 0.3)';
const HUD_IDLE_BUTTON_BG = 'rgba(15, 23, 42, 0.65)';
const HUD_BUTTON_BASE_STYLE: CSSProperties = {
  padding: '4px 8px',
  borderRadius: 6,
  border: '1px solid rgba(148, 163, 184, 0.45)',
  color: '#e2e8f0',
};
const MAP_VIEW_PRESET_BUTTONS: Array<{
  label: string;
  title: string;
  preset: MapViewPreset;
}> = [
  { label: 'Top', title: 'Top view', preset: 'top' },
  { label: 'Front', title: 'Front view', preset: 'front' },
  { label: 'Side', title: 'Right side view', preset: 'right' },
];

function formatRate(value: number): string {
  if (!Number.isFinite(value)) {
    return '0.0';
  }
  return value.toFixed(1);
}

interface ShardHoverBounds {
  minX: number;
  maxX: number;
  minY: number;
  maxY: number;
  area: number;
}

function hudButtonStyle(active = false): CSSProperties {
  return {
    ...HUD_BUTTON_BASE_STYLE,
    background: active ? HUD_ACTIVE_BUTTON_BG : HUD_IDLE_BUTTON_BG,
  };
}

function normalizeShardId(value: string): string {
  return value.trim();
}

function isShardIdentity(value: string): boolean {
  return normalizeShardId(value).startsWith('eShard ');
}

function stableOffsetFromId(id: string): { x: number; y: number } {
  let hash = 2166136261;
  for (let i = 0; i < id.length; i++) {
    hash ^= id.charCodeAt(i);
    hash = Math.imul(hash, 16777619);
  }
  const unsigned = hash >>> 0;
  const angle = ((unsigned % 360) * Math.PI) / 180;
  const radius = 22 + (unsigned % 17);
  return { x: Math.cos(angle) * radius, y: Math.sin(angle) * radius };
}

function getShapeAnchorPoints(shape: ShapeJS): Array<{ x: number; y: number }> {
  const cx = shape.position.x ?? 0;
  const cy = shape.position.y ?? 0;

  if ((shape.type === 'polygon' || shape.type === 'line') && shape.points) {
    if (shape.points.length > 0) {
      return shape.points.map((point) => ({ x: point.x, y: point.y }));
    }
  }

  if (shape.type === 'rectangle' || shape.type === 'rectImage') {
    const halfX = (shape.size?.x ?? 0) / 2;
    const halfY = (shape.size?.y ?? 0) / 2;
    return [
      { x: cx - halfX, y: cy - halfY },
      { x: cx + halfX, y: cy - halfY },
      { x: cx + halfX, y: cy + halfY },
      { x: cx - halfX, y: cy + halfY },
    ];
  }

  if (shape.type === 'circle') {
    const radius = Math.abs(shape.radius ?? 0);
    if (radius > 0) {
      return [
        { x: cx - radius, y: cy },
        { x: cx + radius, y: cy },
        { x: cx, y: cy - radius },
        { x: cx, y: cy + radius },
      ];
    }
  }

  return [{ x: cx, y: cy }];
}

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
  const showShardHoverDetailsRef = useRef(showShardHoverDetails);
  const showGnsConnectionsRef = useRef(showGnsConnections);
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

  const ownerPositions = useMemo(() => {
    const acc = new Map<string, { sumX: number; sumY: number; count: number }>();
    for (const entity of authorityEntities) {
      const ownerId = normalizeShardId(entity.ownerId);
      const current = acc.get(ownerId) ?? { sumX: 0, sumY: 0, count: 0 };
      current.sumX += entity.x;
      current.sumY += entity.y;
      current.count += 1;
      acc.set(ownerId, current);
    }
    const out = new Map<string, { x: number; y: number }>();
    for (const [ownerId, value] of acc) {
      out.set(ownerId, {
        x: value.sumX / Math.max(1, value.count),
        y: value.sumY / Math.max(1, value.count),
      });
    }
    return out;
  }, [authorityEntities]);

  const shardAnchorPositions = useMemo(() => {
    const accumulator = new Map<string, { sumX: number; sumY: number; count: number }>();
    for (const shape of baseShapes) {
      const ownerId = normalizeShardId(shape.ownerId ?? '');
      if (!isShardIdentity(ownerId)) {
        continue;
      }
      const shardId = ownerId;
      const current = accumulator.get(shardId) ?? { sumX: 0, sumY: 0, count: 0 };
      const points = getShapeAnchorPoints(shape);
      for (const point of points) {
        current.sumX += point.x;
        current.sumY += point.y;
        current.count += 1;
      }
      accumulator.set(shardId, current);
    }

    const out = new Map<string, { x: number; y: number }>();
    for (const [shardId, value] of accumulator) {
      if (value.count <= 0) {
        continue;
      }
      out.set(shardId, {
        x: value.sumX / value.count,
        y: value.sumY / value.count,
      });
    }

    return out;
  }, [baseShapes]);

  const mapBoundsCenter = useMemo(() => {
    let minX = Infinity;
    let maxX = -Infinity;
    let minY = Infinity;
    let maxY = -Infinity;
    let hasPoint = false;

    for (const shape of baseShapes) {
      const points = getShapeAnchorPoints(shape);
      for (const point of points) {
        if (!Number.isFinite(point.x) || !Number.isFinite(point.y)) {
          continue;
        }
        hasPoint = true;
        if (point.x < minX) minX = point.x;
        if (point.x > maxX) maxX = point.x;
        if (point.y < minY) minY = point.y;
        if (point.y > maxY) maxY = point.y;
      }
    }

    if (!hasPoint) {
      return { x: 0, y: 0 };
    }
    return {
      x: (minX + maxX) / 2,
      y: (minY + maxY) / 2,
    };
  }, [baseShapes]);

  const shardBoundsById = useMemo(() => {
    const raw = new Map<
      string,
      { minX: number; maxX: number; minY: number; maxY: number; hasPoint: boolean }
    >();

    for (const shape of baseShapes) {
      const ownerId = normalizeShardId(shape.ownerId ?? '');
      if (!isShardIdentity(ownerId)) {
        continue;
      }
      const points = getShapeAnchorPoints(shape);
      if (points.length === 0) {
        continue;
      }
      const current = raw.get(ownerId) ?? {
        minX: Infinity,
        maxX: -Infinity,
        minY: Infinity,
        maxY: -Infinity,
        hasPoint: false,
      };
      for (const point of points) {
        if (!Number.isFinite(point.x) || !Number.isFinite(point.y)) {
          continue;
        }
        current.hasPoint = true;
        if (point.x < current.minX) current.minX = point.x;
        if (point.x > current.maxX) current.maxX = point.x;
        if (point.y < current.minY) current.minY = point.y;
        if (point.y > current.maxY) current.maxY = point.y;
      }
      raw.set(ownerId, current);
    }

    const out = new Map<string, ShardHoverBounds>();
    for (const [shardId, bounds] of raw) {
      if (!bounds.hasPoint) {
        continue;
      }
      const padding = 2;
      const minX = bounds.minX - padding;
      const maxX = bounds.maxX + padding;
      const minY = bounds.minY - padding;
      const maxY = bounds.maxY + padding;
      out.set(shardId, {
        minX,
        maxX,
        minY,
        maxY,
        area: Math.max(1, (maxX - minX) * (maxY - minY)),
      });
    }
    return out;
  }, [baseShapes]);

  const networkNodeIds = useMemo(() => {
    // Include both telemetry rows and valid connection targets so shard nodes
    // track live GNS topology updates.
    const ids = new Set<string>();
    for (const shard of networkTelemetry) {
      const shardId = normalizeShardId(shard.shardId);
      if (shardId.length > 0 && isShardIdentity(shardId)) {
        ids.add(shardId);
      }
      for (const connection of shard.connections) {
        const targetId = normalizeShardId(connection.targetId);
        if (targetId.length > 0 && isShardIdentity(targetId)) {
          ids.add(targetId);
        }
      }
    }
    return Array.from(ids.values()).sort();
  }, [networkTelemetry]);
  const networkNodeIdSet = useMemo(
    () => new Set(networkNodeIds),
    [networkNodeIds]
  );

  const projectedShardPositions = useMemo(() => {
    const out = new Map<string, { x: number; y: number }>();

    // Keep node positions shard-centered. Entity links should not re-layout nodes.
    for (const shardId of networkNodeIds) {
      const pos = shardAnchorPositions.get(shardId);
      if (pos) {
        out.set(shardId, pos);
      }
    }

    const unresolvedIds = networkNodeIds
      .filter((id: string) => !out.has(id))
      .sort();
    if (unresolvedIds.length === 0) {
      return out;
    }

    // Fallback placement for shards missing map anchors.
    // Deterministic per shard id and independent from entity movement.
    let centerX = 0;
    let centerY = 0;
    let centerCount = 0;
    for (const pos of out.values()) {
      centerX += pos.x;
      centerY += pos.y;
      centerCount += 1;
    }
    if (centerCount > 0) {
      centerX /= centerCount;
      centerY /= centerCount;
    } else {
      centerX = mapBoundsCenter.x;
      centerY = mapBoundsCenter.y;
    }

    for (const unresolvedId of unresolvedIds) {
      const offset = stableOffsetFromId(unresolvedId);
      out.set(unresolvedId, {
        x: centerX + offset.x,
        y: centerY + offset.y,
      });
    }

    return out;
  }, [mapBoundsCenter, networkNodeIds, shardAnchorPositions]);

  const shardHoverBoundsById = useMemo(() => {
    const out = new Map<string, ShardHoverBounds>(shardBoundsById);
    const fallbackHalfSize = 8;
    for (const shardId of networkNodeIds) {
      if (out.has(shardId)) {
        continue;
      }
      const anchor = projectedShardPositions.get(shardId);
      if (!anchor) {
        continue;
      }
      const minX = anchor.x - fallbackHalfSize;
      const maxX = anchor.x + fallbackHalfSize;
      const minY = anchor.y - fallbackHalfSize;
      const maxY = anchor.y + fallbackHalfSize;
      out.set(shardId, {
        minX,
        maxX,
        minY,
        maxY,
        area: (maxX - minX) * (maxY - minY),
      });
    }
    return out;
  }, [networkNodeIds, projectedShardPositions, shardBoundsById]);

  const networkEdgeCount = useMemo(() => {
    const seen = new Set<string>();
    for (const shard of networkTelemetry) {
      const fromId = normalizeShardId(shard.shardId);
      if (!networkNodeIdSet.has(fromId)) {
        continue;
      }
      for (const connection of shard.connections) {
        const toId = normalizeShardId(connection.targetId);
        if (!networkNodeIdSet.has(toId)) {
          continue;
        }
        const key = fromId < toId ? `${fromId}|${toId}` : `${toId}|${fromId}`;
        seen.add(key);
      }
    }
    return seen.size;
  }, [networkTelemetry, networkNodeIdSet]);

  const shardTelemetryById = useMemo(() => {
    const out = new Map<string, (typeof networkTelemetry)[number]>();
    for (const shard of networkTelemetry) {
      const shardId = normalizeShardId(shard.shardId);
      if (!isShardIdentity(shardId)) {
        continue;
      }
      out.set(shardId, shard);
    }
    return out;
  }, [networkTelemetry]);

  const hoveredShardEdgeLabels = useMemo(
    () =>
      hoveredShardId == null
        ? []
        : (() => {
            const shard = shardTelemetryById.get(hoveredShardId);
            const fromPos = projectedShardPositions.get(hoveredShardId);
            if (!shard || !fromPos) {
              return [];
            }
            const edgeRates = new Map<
              string,
              { inBytesPerSec: number; outBytesPerSec: number }
            >();
            for (const connection of shard.connections) {
              const targetId = normalizeShardId(connection.targetId);
              if (!networkNodeIdSet.has(targetId)) {
                continue;
              }
              const existing = edgeRates.get(targetId) ?? {
                inBytesPerSec: 0,
                outBytesPerSec: 0,
              };
              existing.inBytesPerSec += Number.isFinite(connection.inBytesPerSec)
                ? connection.inBytesPerSec
                : 0;
              existing.outBytesPerSec += Number.isFinite(connection.outBytesPerSec)
                ? connection.outBytesPerSec
                : 0;
              edgeRates.set(targetId, existing);
            }
            return Array.from(edgeRates.entries())
              .map(([targetId, rates]) => {
                const toPos = projectedShardPositions.get(targetId);
                if (!toPos) {
                  return null;
                }
                return {
                  targetId,
                  from: fromPos,
                  to: toPos,
                  text: `In ${formatRate(rates.inBytesPerSec)} B/s • Out ${formatRate(
                    rates.outBytesPerSec
                  )} B/s`,
                };
              })
              .filter((value): value is NonNullable<typeof value> => value != null)
              .sort((a, b) => a.targetId.localeCompare(b.targetId));
          })(),
    [hoveredShardId, networkNodeIdSet, projectedShardPositions, shardTelemetryById]
  );

  useEffect(() => {
    showShardHoverDetailsRef.current = showShardHoverDetails;
    if (!showShardHoverDetails) {
      setHoveredShardId(null);
      setHoveredShardAnchor(null);
      return;
    }
    rendererRef.current?.draw();
  }, [showShardHoverDetails]);

  const shardHoverBoundsByIdRef = useRef<Map<string, ShardHoverBounds>>(new Map());
  const networkNodeIdSetRef = useRef<Set<string>>(new Set());

  useEffect(() => {
    showGnsConnectionsRef.current = showGnsConnections;
  }, [showGnsConnections]);

  useEffect(() => {
    networkNodeIdSetRef.current = networkNodeIdSet;
  }, [networkNodeIdSet]);

  useEffect(() => {
    shardHoverBoundsByIdRef.current = shardHoverBoundsById;
  }, [shardHoverBoundsById]);

  useEffect(() => {
    if (!showGnsConnections) {
      setHoveredShardId(null);
      setHoveredShardAnchor(null);
      return;
    }
    if (hoveredShardId && !networkNodeIdSet.has(hoveredShardId)) {
      setHoveredShardId(null);
      setHoveredShardAnchor(null);
    }
  }, [hoveredShardId, networkNodeIdSet, showGnsConnections]);

  useEffect(() => {
    rendererRef.current?.setHoverEdgeLabels(
      showShardHoverDetails && showGnsConnections ? hoveredShardEdgeLabels : []
    );
  }, [hoveredShardEdgeLabels, showGnsConnections, showShardHoverDetails]);

  const handleMapPointerWorld = useCallback(
    (
      point: { x: number; y: number } | null,
      screen: { x: number; y: number } | null
    ) => {
      if (
        !showShardHoverDetailsRef.current ||
        !showGnsConnectionsRef.current ||
        !point ||
        !screen
      ) {
        setHoveredShardId(null);
        setHoveredShardAnchor(null);
        return;
      }

      let hoveredId: string | null = null;
      let smallestArea = Infinity;
      for (const [shardId, bounds] of shardHoverBoundsByIdRef.current) {
        if (!networkNodeIdSetRef.current.has(shardId)) {
          continue;
        }
        if (
          point.x < bounds.minX ||
          point.x > bounds.maxX ||
          point.y < bounds.minY ||
          point.y > bounds.maxY
        ) {
          continue;
        }
        if (bounds.area < smallestArea) {
          smallestArea = bounds.area;
          hoveredId = shardId;
        }
      }

      if (!hoveredId) {
        setHoveredShardId(null);
        setHoveredShardAnchor(null);
        return;
      }

      setHoveredShardId((prev) => (prev === hoveredId ? prev : hoveredId));
      setHoveredShardAnchor((prev) => {
        if (
          !prev ||
          Math.abs(prev.x - screen.x) > 0.5 ||
          Math.abs(prev.y - screen.y) > 0.5
        ) {
          return screen;
        }
        return prev;
      });
    },
    []
  );

  const overlayShapes = useMemo<ShapeJS[]>(() => {
    const overlays: ShapeJS[] = [];

    if (showAuthorityEntities) {
      for (const entity of authorityEntities) {
        // Entity dot
        overlays.push({
          type: 'circle',
          position: { x: entity.x, y: entity.y },
          radius: 1.8,
          color: 'rgba(255, 240, 80, 1)',
        });

        // Owner link (entity -> owner centroid)
        const ownerPos = projectedShardPositions.get(
          normalizeShardId(entity.ownerId)
        );
        if (ownerPos) {
          overlays.push({
            type: 'line',
            position: { x: 0, y: 0 },
            points: [
              { x: entity.x, y: entity.y },
              { x: ownerPos.x, y: ownerPos.y },
            ],
            color: 'rgba(255, 255, 0, 0.9)',
          });
        }
      }
    }

    if (showGnsConnections) {
      const seen = new Set<string>();
      for (const shard of networkTelemetry) {
        const fromId = normalizeShardId(shard.shardId);
        if (!networkNodeIdSet.has(fromId)) {
          continue;
        }
        const fromPos = projectedShardPositions.get(fromId);
        if (!fromPos) continue;
        for (const connection of shard.connections) {
          const toId = normalizeShardId(connection.targetId);
          if (!networkNodeIdSet.has(toId)) {
            continue;
          }
          const toPos = projectedShardPositions.get(toId);
          if (!toPos) continue;

          // Undirected dedupe for display clarity
          const key =
            fromId < toId ? `${fromId}|${toId}` : `${toId}|${fromId}`;
          if (seen.has(key)) continue;
          seen.add(key);

          overlays.push({
            type: 'line',
            position: { x: 0, y: 0 },
            points: [
              { x: fromPos.x, y: fromPos.y },
              { x: toPos.x, y: toPos.y },
            ],
            color: 'rgba(80, 200, 255, 0.65)',
          });
        }
      }

      // Draw shard node dots from the same source as network page.
      for (const shardId of networkNodeIds) {
        const anchor = projectedShardPositions.get(shardId);
        if (!anchor) {
          continue;
        }
        const hasOwnerSample = ownerPositions.has(shardId);
        overlays.push({
          type: 'circle',
          position: anchor,
          radius: hasOwnerSample ? 2.6 : 2.1,
          color: hasOwnerSample
            ? 'rgba(80, 200, 255, 0.95)'
            : 'rgba(120, 170, 220, 0.75)',
        });
      }
    }

    return overlays;
  }, [
    authorityEntities,
    networkTelemetry,
    networkNodeIds,
    networkNodeIdSet,
    ownerPositions,
    projectedShardPositions,
    showAuthorityEntities,
    showGnsConnections,
  ]);

  const combinedShapes = useMemo(
    () => [...baseShapes, ...overlayShapes],
    [baseShapes, overlayShapes]
  );

  // Init renderer once
  useEffect(() => {
    const container = containerRef.current;
    if (!container || rendererRef.current) {
      return;
    }
    rendererRef.current = createMapRenderer({
      container,
      shapes: [],
      viewMode,
      projectionMode,
      interactionSensitivity,
      onPointerWorldPosition: handleMapPointerWorld,
    });
    return () => {
      rendererRef.current?.destroy();
      rendererRef.current = null;
    };
  }, [handleMapPointerWorld]);

  // Push shape updates to renderer
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
      >
        {showShardHoverDetails && hoveredShardId && hoveredShardAnchor && (
          <div
            style={{
              position: 'absolute',
              left: hoveredShardAnchor.x + 12,
              top: hoveredShardAnchor.y - 8,
              transform: 'translateY(-50%)',
              maxWidth: 280,
              padding: '8px 10px',
              borderRadius: 8,
              border: '1px solid rgba(148, 163, 184, 0.4)',
              background: 'rgba(15, 23, 42, 0.9)',
              color: '#e2e8f0',
              fontSize: 11,
              lineHeight: 1.35,
              pointerEvents: 'none',
              zIndex: 20,
              boxShadow: '0 8px 24px rgba(2, 6, 23, 0.35)',
            }}
          >
            <div
              style={{
                fontSize: 12,
                color: '#f8fafc',
                marginBottom: 4,
                whiteSpace: 'nowrap',
                overflow: 'hidden',
                textOverflow: 'ellipsis',
              }}
              title={hoveredShardId}
            >
              {hoveredShardId}
            </div>
            <div style={{ color: '#cbd5e1', marginBottom: 4 }}>
              Down {formatRate(shardTelemetryById.get(hoveredShardId)?.downloadKbps ?? 0)} B/s
              {' • '}
              Up {formatRate(shardTelemetryById.get(hoveredShardId)?.uploadKbps ?? 0)} B/s
            </div>
            <div style={{ color: '#94a3b8' }}>
              {hoveredShardEdgeLabels.length} outgoing connections
            </div>
          </div>
        )}
      </div>

      <div
        style={{
          flex: '0 0 auto',
          background: 'rgba(15, 23, 42, 0.92)',
          color: '#e2e8f0',
          borderTop: '1px solid rgba(148, 163, 184, 0.35)',
          padding: '10px 14px 12px',
          display: 'flex',
          gap: 12,
          alignItems: 'center',
          flexWrap: 'wrap',
          fontSize: 13,
          backdropFilter: 'blur(6px)',
        }}
      >
        <label style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
          <input
            type="checkbox"
            checked={showGnsConnections}
            onChange={() => setShowGnsConnections(!showGnsConnections)}
          />
          GNS connections
        </label>
        <label style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
          <input
            type="checkbox"
            checked={showAuthorityEntities}
            onChange={() => setShowAuthorityEntities(!showAuthorityEntities)}
          />
          entities + owner links
        </label>
        <label style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
          <input
            type="checkbox"
            checked={showShardHoverDetails}
            onChange={() => setShowShardHoverDetails(!showShardHoverDetails)}
          />
          shard hover details
        </label>
        <span style={{ opacity: 0.8 }}>
          entities: {authorityEntities.length} | shards: {networkNodeIds.length}
        </span>
        <span style={{ opacity: 0.8 }}>
          connections: {networkEdgeCount} | claimed entities: {authorityEntities.length}
        </span>
        <div style={{ display: 'inline-flex', alignItems: 'center', gap: 8 }}>
          <div
            style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: 6,
              padding: '4px',
              borderRadius: 8,
              border: '1px solid rgba(148, 163, 184, 0.45)',
              background: 'rgba(2, 6, 23, 0.5)',
            }}
          >
            <button
              type="button"
              onClick={() => setViewMode('2d')}
              style={hudButtonStyle(viewMode === '2d')}
              title="2D mode: drag to pan, mouse wheel to zoom"
            >
              2D
            </button>
            <button
              type="button"
              onClick={() => {
                setViewMode('3d');
                setProjectionMode('orthographic');
              }}
              style={hudButtonStyle(
                viewMode === '3d' && projectionMode === 'orthographic'
              )}
              title="3D Orthographic mode"
            >
              3D Ortho
            </button>
            <button
              type="button"
              onClick={() => {
                setViewMode('3d');
                setProjectionMode('perspective');
              }}
              style={hudButtonStyle(
                viewMode === '3d' && projectionMode === 'perspective'
              )}
              title="3D Perspective mode"
            >
              3D Persp
            </button>
          </div>
          <div
            style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: 6,
              padding: '4px',
              borderRadius: 8,
              border: '1px solid rgba(148, 163, 184, 0.45)',
              background: 'rgba(2, 6, 23, 0.45)',
            }}
          >
            <span style={{ opacity: 0.72, fontSize: 11, padding: '0 4px' }}>
              Views
            </span>
            {MAP_VIEW_PRESET_BUTTONS.map((button) => (
              <button
                key={button.preset}
                type="button"
                onClick={() => snapToPreset(button.preset)}
                style={hudButtonStyle()}
                title={button.title}
              >
                {button.label}
              </button>
            ))}
          </div>
        </div>
        <span style={{ opacity: 0.85, fontSize: 12 }}>
          {viewMode === '2d'
            ? '2D controls: LMB pan, wheel zoom, WASD map X/Y, F frame'
            : '3D controls: RMB orbit, LMB pan, wheel zoom, WASD camera-relative, F frame'}
        </span>
        <label
          style={{
            display: 'flex',
            gap: 8,
            alignItems: 'center',
          }}
        >
          sensitivity
          <input
            type="number"
            min={MIN_INTERACTION_SENSITIVITY}
            max={MAX_INTERACTION_SENSITIVITY}
            step={0.1}
            value={interactionSensitivity}
            onChange={(e) =>
              setInteractionSensitivity(
                Math.max(
                  MIN_INTERACTION_SENSITIVITY,
                  Math.min(MAX_INTERACTION_SENSITIVITY, Number(e.target.value))
                )
              )
            }
            style={{
              width: 68,
              borderRadius: 6,
              border: '1px solid rgba(148, 163, 184, 0.45)',
              background: 'rgba(2, 6, 23, 0.45)',
              color: '#e2e8f0',
              padding: '2px 6px',
            }}
          />
        </label>
        <label
          style={{
            display: 'flex',
            gap: 8,
            alignItems: 'center',
            minWidth: 220,
          }}
        >
          <input
            type="range"
            min={MIN_POLL_INTERVAL_MS}
            max={MAX_POLL_INTERVAL_MS}
            step={50}
            value={pollIntervalMs}
            onChange={(e) =>
              setPollIntervalMs(
                Math.max(
                  MIN_POLL_INTERVAL_MS,
                  Math.min(MAX_POLL_INTERVAL_MS, Number(e.target.value))
                )
              )
            }
          />
          poll: {pollIntervalMs}ms
        </label>
      </div>
    </div>
  );
}
