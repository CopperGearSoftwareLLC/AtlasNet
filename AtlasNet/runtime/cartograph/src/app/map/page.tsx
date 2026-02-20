'use client';

import { useEffect, useMemo, useRef, useState } from 'react';
import type { ShapeJS } from '../lib/cartographTypes';
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

const DEFAULT_POLL_INTERVAL_MS = 200;
const MIN_POLL_INTERVAL_MS = 50;
const MAX_POLL_INTERVAL_MS = 1000;

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
  const [viewMode, setViewMode] = useState<MapViewMode>('2d');
  const [projectionMode, setProjectionMode] =
    useState<MapProjectionMode>('orthographic');
  const [pollIntervalMs, setPollIntervalMs] = useState(DEFAULT_POLL_INTERVAL_MS);
  const baseShapes = useHeuristicShapes();
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

  const networkNodeIds = useMemo(() => {
    // Authoritative shard ids come from telemetry rows, not connection targets.
    // This avoids rendering nodes for malformed/stale target ids.
    const ids = new Set<string>();
    for (const shard of networkTelemetry) {
      const shardId = normalizeShardId(shard.shardId);
      if (shardId.length > 0 && isShardIdentity(shardId)) {
        ids.add(shardId);
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
    }

    for (const unresolvedId of unresolvedIds) {
      const offset = stableOffsetFromId(unresolvedId);
      out.set(unresolvedId, {
        x: centerX + offset.x,
        y: centerY + offset.y,
      });
    }

    return out;
  }, [networkNodeIds, shardAnchorPositions]);

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
    });
    return () => {
      rendererRef.current?.destroy();
      rendererRef.current = null;
    };
  }, []);

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

  return (
    <div style={{ width: '100%', height: '100%', position: 'relative' }}>
      <div
        style={{
          position: 'absolute',
          top: 12,
          left: 12,
          zIndex: 10,
          background: 'rgba(15, 23, 42, 0.82)',
          color: '#e2e8f0',
          border: '1px solid rgba(148, 163, 184, 0.45)',
          borderRadius: 10,
          padding: '8px 10px',
          display: 'flex',
          gap: 14,
          alignItems: 'center',
          fontSize: 13,
          backdropFilter: 'blur(4px)',
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
        <span style={{ opacity: 0.8 }}>
          entities: {authorityEntities.length} | shards: {networkNodeIds.length}
        </span>
        <span style={{ opacity: 0.8 }}>
          connections: {networkEdgeCount} | claimed entities: {authorityEntities.length}
        </span>
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
            style={{
              padding: '4px 8px',
              borderRadius: 6,
              border: '1px solid rgba(148, 163, 184, 0.45)',
              background:
                viewMode === '2d'
                  ? 'rgba(56, 189, 248, 0.3)'
                  : 'rgba(15, 23, 42, 0.65)',
              color: '#e2e8f0',
            }}
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
            style={{
              padding: '4px 8px',
              borderRadius: 6,
              border: '1px solid rgba(148, 163, 184, 0.45)',
              background:
                viewMode === '3d' && projectionMode === 'orthographic'
                  ? 'rgba(56, 189, 248, 0.3)'
                  : 'rgba(15, 23, 42, 0.65)',
              color: '#e2e8f0',
            }}
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
            style={{
              padding: '4px 8px',
              borderRadius: 6,
              border: '1px solid rgba(148, 163, 184, 0.45)',
              background:
                viewMode === '3d' && projectionMode === 'perspective'
                  ? 'rgba(56, 189, 248, 0.3)'
                  : 'rgba(15, 23, 42, 0.65)',
              color: '#e2e8f0',
            }}
            title="3D Perspective mode"
          >
            3D Persp
          </button>
          <button
            type="button"
            onClick={() => rendererRef.current?.resetCamera()}
            style={{
              padding: '4px 8px',
              borderRadius: 6,
              border: '1px solid rgba(148, 163, 184, 0.45)',
              background: 'rgba(15, 23, 42, 0.65)',
              color: '#e2e8f0',
            }}
            title="Frame scene (Unity-style F)"
          >
            Frame
          </button>
        </div>
        <span style={{ opacity: 0.85, fontSize: 12 }}>
          {viewMode === '2d'
            ? '2D controls: drag pan, wheel zoom, F frame'
            : '3D controls: RMB or Alt+LMB orbit, MMB pan, wheel zoom, F frame'}
        </span>
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

      <div
        ref={containerRef}
        style={{
          width: '100%',
          height: '100%',
          overflow: 'hidden',
          touchAction: 'none',
          border: '1px solid #ccc',
        }}
      />
    </div>
  );
}
