'use client';

import { useMemo } from 'react';
import type { Point2, ShardHoverBounds } from '../../map/core/mapData';
import { formatPingMs } from '../../shared/networkTelemetryMetrics';
import type { ServerBoundsShardSummary } from './serverBoundsTypes';

interface ServerBoundsMinimapSectionProps {
  shardSummaries: ServerBoundsShardSummary[];
  boundsByShardId: Map<string, ShardHoverBounds>;
  shardPolygonsById: Map<string, Point2[][]>;
  claimedBoundShardIds: Set<string>;
  title?: string;
  emptyMessage?: string;
}

function formatRate(value: number): string {
  if (!Number.isFinite(value)) {
    return '0.0';
  }
  return value.toFixed(1);
}

function formatArea(value: number): string {
  if (!Number.isFinite(value) || value <= 0) {
    return '-';
  }
  if (value >= 1_000_000) {
    return `${(value / 1_000_000).toFixed(2)}M`;
  }
  if (value >= 1_000) {
    return `${(value / 1_000).toFixed(1)}k`;
  }
  return value.toFixed(0);
}

function computeGlobalBounds(boundsByShardId: Map<string, ShardHoverBounds>): ShardHoverBounds | null {
  let minX = Infinity;
  let maxX = -Infinity;
  let minY = Infinity;
  let maxY = -Infinity;
  let hasBounds = false;

  for (const bounds of boundsByShardId.values()) {
    hasBounds = true;
    if (bounds.minX < minX) minX = bounds.minX;
    if (bounds.maxX > maxX) maxX = bounds.maxX;
    if (bounds.minY < minY) minY = bounds.minY;
    if (bounds.maxY > maxY) maxY = bounds.maxY;
  }

  if (!hasBounds) {
    return null;
  }

  return {
    minX,
    maxX,
    minY,
    maxY,
    area: Math.max(1, (maxX - minX) * (maxY - minY)),
  };
}

function MiniShardMap({
  boundsByShardId,
  shardPolygonsById,
  claimedBoundShardIds,
  focusShardId,
  highlightFocus,
}: {
  boundsByShardId: Map<string, ShardHoverBounds>;
  shardPolygonsById: Map<string, Point2[][]>;
  claimedBoundShardIds: Set<string>;
  focusShardId: string;
  highlightFocus: boolean;
}) {
  const visibleBoundsByShardId = useMemo(() => {
    const out = new Map<string, ShardHoverBounds>();
    for (const [shardId, bounds] of boundsByShardId) {
      if (claimedBoundShardIds.has(shardId)) {
        out.set(shardId, bounds);
      }
    }
    return out;
  }, [boundsByShardId, claimedBoundShardIds]);

  const visiblePolygonsByShardId = useMemo(() => {
    const out = new Map<string, Point2[][]>();
    for (const [shardId, polygons] of shardPolygonsById) {
      if (!claimedBoundShardIds.has(shardId) || !Array.isArray(polygons)) {
        continue;
      }
      const validPolygons: Point2[][] = [];
      for (const polygon of polygons) {
        const cleanPolygon = polygon.filter(
          (point) => Number.isFinite(point.x) && Number.isFinite(point.y)
        );
        if (cleanPolygon.length >= 3) {
          validPolygons.push(cleanPolygon);
        }
      }
      if (validPolygons.length > 0) {
        out.set(shardId, validPolygons);
      }
    }
    return out;
  }, [claimedBoundShardIds, shardPolygonsById]);

  const globalBounds = useMemo(
    () => computeGlobalBounds(visibleBoundsByShardId),
    [visibleBoundsByShardId]
  );

  if (!globalBounds) {
    return (
      <div className="flex h-[118px] items-center justify-center rounded-lg border border-slate-800 bg-slate-950 text-xs text-slate-500">
        No bounds yet
      </div>
    );
  }

  const width = 200;
  const height = 118;
  const pad = 8;
  const spanX = Math.max(1, globalBounds.maxX - globalBounds.minX);
  const spanY = Math.max(1, globalBounds.maxY - globalBounds.minY);
  const scale = Math.min((width - pad * 2) / spanX, (height - pad * 2) / spanY);
  const contentWidth = spanX * scale;
  const contentHeight = spanY * scale;
  const offsetX = (width - contentWidth) / 2;
  const offsetY = (height - contentHeight) / 2;

  function toX(x: number): number {
    return offsetX + (x - globalBounds.minX) * scale;
  }

  function toY(y: number): number {
    return height - (offsetY + (y - globalBounds.minY) * scale);
  }

  return (
    <svg
      viewBox={`0 0 ${width} ${height}`}
      className="h-[118px] w-full rounded-lg border border-slate-800 bg-slate-950"
      role="img"
      aria-label={`Bounds minimap for ${focusShardId}`}
    >
      {Array.from(visibleBoundsByShardId.entries()).map(([shardId, bounds]) => {
        const isFocus = highlightFocus && shardId === focusShardId;
        const polygons = visiblePolygonsByShardId.get(shardId) ?? [];
        const fill = isFocus ? 'rgba(56, 189, 248, 0.28)' : 'rgba(100, 116, 139, 0.08)';
        const stroke = isFocus ? 'rgba(56, 189, 248, 0.95)' : 'rgba(71, 85, 105, 0.55)';
        const strokeWidth = isFocus ? 1.8 : 1;

        if (polygons.length > 0) {
          return (
            <g key={shardId}>
              {polygons.map((polygon, index) => (
                <polygon
                  key={`${shardId}:${index}`}
                  points={polygon.map((point) => `${toX(point.x)},${toY(point.y)}`).join(' ')}
                  fill={fill}
                  stroke={stroke}
                  strokeWidth={strokeWidth}
                  strokeLinejoin="round"
                />
              ))}
            </g>
          );
        }

        const x1 = toX(bounds.minX);
        const x2 = toX(bounds.maxX);
        const y1 = toY(bounds.minY);
        const y2 = toY(bounds.maxY);
        const left = Math.min(x1, x2);
        const top = Math.min(y1, y2);
        const rectWidth = Math.max(1, Math.abs(x2 - x1));
        const rectHeight = Math.max(1, Math.abs(y2 - y1));

        return (
          <rect
            key={shardId}
            x={left}
            y={top}
            width={rectWidth}
            height={rectHeight}
            fill={fill}
            stroke={stroke}
            strokeWidth={strokeWidth}
          />
        );
      })}
    </svg>
  );
}

function ShardSummaryCard({
  summary,
  boundsByShardId,
  shardPolygonsById,
  claimedBoundShardIds,
}: {
  summary: ServerBoundsShardSummary;
  boundsByShardId: Map<string, ShardHoverBounds>;
  shardPolygonsById: Map<string, Point2[][]>;
  claimedBoundShardIds: Set<string>;
}) {
  return (
    <article className="rounded-2xl border border-slate-800 bg-slate-900/70 p-3">
      <div className="mb-2 flex items-start justify-between gap-2">
        <div>
          <h3 className="font-mono text-sm text-slate-100">{summary.shardId}</h3>
          <p className="text-xs text-slate-500">bounds area {formatArea(summary.area)}</p>
        </div>
        <span className="rounded-full border border-slate-700 bg-slate-950 px-2 py-1 text-[10px] uppercase tracking-wide text-slate-300">
          {summary.status}
        </span>
      </div>

      <MiniShardMap
        boundsByShardId={boundsByShardId}
        shardPolygonsById={shardPolygonsById}
        claimedBoundShardIds={claimedBoundShardIds}
        focusShardId={summary.shardId}
        highlightFocus={summary.hasClaimedBound}
      />

      <div className="mt-3 grid grid-cols-2 gap-2 text-xs">
        <div className="rounded border border-slate-800 bg-slate-950 px-2 py-1 text-slate-300">
          clients: <span className="font-mono">{summary.clientCount}</span>
        </div>
        <div className="rounded border border-slate-800 bg-slate-950 px-2 py-1 text-slate-300">
          entities: <span className="font-mono">{summary.entityCount}</span>
        </div>
        <div className="rounded border border-slate-800 bg-slate-950 px-2 py-1 text-slate-300">
          in bytes: <span className="font-mono">{formatRate(summary.downloadKbps)}</span>
        </div>
        <div className="rounded border border-slate-800 bg-slate-950 px-2 py-1 text-slate-300">
          out bytes: <span className="font-mono">{formatRate(summary.uploadKbps)}</span>
        </div>
        <div className="rounded border border-slate-800 bg-slate-950 px-2 py-1 text-slate-300">
          avg ping: <span className="font-mono">{formatPingMs(summary.avgPingMs)}</span>
        </div>
      </div>
    </article>
  );
}

export function ServerBoundsMinimapSection({
  shardSummaries,
  boundsByShardId,
  shardPolygonsById,
  claimedBoundShardIds,
  title = 'Server Bounds Minimap',
  emptyMessage = 'Waiting for shard telemetry and map bounds...',
}: ServerBoundsMinimapSectionProps) {
  return (
    <section className="space-y-3">
      <div className="flex items-center justify-between">
        <h2 className="text-lg font-semibold text-slate-100">{title}</h2>
        <p className="text-xs text-slate-500">{shardSummaries.length} servers</p>
      </div>

      {shardSummaries.length === 0 ? (
        <div className="rounded-2xl border border-slate-800 bg-slate-900/70 p-4 text-sm text-slate-500">
          {emptyMessage}
        </div>
      ) : (
        <div className="grid grid-cols-1 gap-3 md:grid-cols-2 2xl:grid-cols-3">
          {shardSummaries.map((summary) => (
            <ShardSummaryCard
              key={summary.shardId}
              summary={summary}
              boundsByShardId={boundsByShardId}
              shardPolygonsById={shardPolygonsById}
              claimedBoundShardIds={claimedBoundShardIds}
            />
          ))}
        </div>
      )}
    </section>
  );
}
