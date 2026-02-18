'use client';

import { useEffect, useMemo, useRef, useState } from 'react';
import type { ShardTelemetry } from '../lib/networkTelemetryTypes';
import {
  parseAuthorityRows,
  type AuthorityEntityTelemetry,
} from '../lib/authorityTelemetryTypes';
import { createMapRenderer } from '../lib/mapRenderer';
import type { ShapeJS } from '../lib/types';

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
  const [baseShapes, setBaseShapes] = useState<ShapeJS[]>([]);
  const [networkTelemetry, setNetworkTelemetry] = useState<ShardTelemetry[]>([]);
  const [authorityEntities, setAuthorityEntities] = useState<
    AuthorityEntityTelemetry[]
  >([]);
  const [showGnsConnections, setShowGnsConnections] = useState(false);
  const [showAuthorityEntities, setShowAuthorityEntities] = useState(true);

  // Fetch static heuristic shapes (base map)
  useEffect(() => {
    let alive = true;
    fetch('/api/heuristicfetch')
      .then((res) => res.json())
      .then((data: ShapeJS[]) => {
        if (!alive) return;
        setBaseShapes(Array.isArray(data) ? data : []);
      })
      .catch(console.error);
    return () => {
      alive = false;
    };
  }, []);

  // Poll network telemetry for GNS overlay
  useEffect(() => {
    let alive = true;
    async function poll() {
      try {
        const res = await fetch('/api/networktelemetry', { cache: 'no-store' });
        if (!res.ok) return;
        const data = (await res.json()) as ShardTelemetry[];
        if (!alive) return;
        setNetworkTelemetry(Array.isArray(data) ? data : []);
      } catch {
        if (!alive) return;
        setNetworkTelemetry([]);
      }
    }
    poll();
    const id = setInterval(poll, 500);
    return () => {
      alive = false;
      clearInterval(id);
    };
  }, []);

  // Poll authority telemetry rows for entity overlay
  useEffect(() => {
    let alive = true;
    async function poll() {
      try {
        const res = await fetch('/api/authoritytelemetry', { cache: 'no-store' });
        if (!res.ok) return;
        const raw = await res.json();
        if (!alive) return;
        setAuthorityEntities(parseAuthorityRows(raw));
      } catch {
        if (!alive) return;
        setAuthorityEntities([]);
      }
    }
    poll();
    const id = setInterval(poll, 500);
    return () => {
      alive = false;
      clearInterval(id);
    };
  }, []);

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
    return Array.from(ids.values());
  }, [networkTelemetry]);

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
      if (!isShardIdentity(fromId)) {
        continue;
      }
      for (const connection of shard.connections) {
        const toId = normalizeShardId(connection.targetId);
        if (!isShardIdentity(toId)) {
          continue;
        }
        const key = fromId < toId ? `${fromId}|${toId}` : `${toId}|${fromId}`;
        seen.add(key);
      }
    }
    return seen.size;
  }, [networkTelemetry]);

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
        if (!isShardIdentity(fromId)) {
          continue;
        }
        const fromPos = projectedShardPositions.get(fromId);
        if (!fromPos) continue;
        for (const connection of shard.connections) {
          const toId = normalizeShardId(connection.targetId);
          if (!isShardIdentity(toId)) {
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
    rendererRef.current = createMapRenderer({ container, shapes: [] });
    return () => {
      rendererRef.current = null;
      container.innerHTML = '';
    };
  }, []);

  // Push shape updates to renderer
  useEffect(() => {
    rendererRef.current?.setShapes(combinedShapes);
  }, [combinedShapes]);

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
