'use client';

import { useEffect, useMemo, useRef, useState } from 'react';
import type { MouseEvent, WheelEvent } from 'react';
import type { GraphEdge, GraphNode, GraphNodeStats } from '../lib/networkGraph';

const MIN_ZOOM = 0.3;
const MAX_ZOOM = 3;
const ZOOM_IN_FACTOR = 1.1;
const ZOOM_OUT_FACTOR = 0.9;
const DEFAULT_NODE_RADIUS = 12;
const SELECTED_NODE_RADIUS = 16;

type CircularNodeGraphProps = {
  width: number;
  height: number;
  nodes: GraphNode[];
  edges: GraphEdge[];
  focusedNodeId?: string | null;
  selectedNodeId?: string | null;
  onNodeHover?: (nodeId: string | null) => void;
  onNodeSelect?: (nodeId: string) => void;
  nodeStats?: Map<string, GraphNodeStats>;
  resetViewToken?: number;
};

function formatRate(value: number): string {
  if (!Number.isFinite(value)) {
    return '0';
  }
  return value.toFixed(1);
}

function getNodeRadius(selectedNodeId: string | null | undefined, nodeId: string): number {
  return selectedNodeId === nodeId ? SELECTED_NODE_RADIUS : DEFAULT_NODE_RADIUS;
}

function edgeColor(edge: GraphEdge): string {
  const key = `${edge.from}->${edge.to}`;
  let hash = 0;
  for (let i = 0; i < key.length; i += 1) {
    hash = (hash * 31 + key.charCodeAt(i)) | 0;
  }
  const r = (hash & 0xff0000) >> 16;
  const g = (hash & 0x00ff00) >> 8;
  const b = hash & 0x0000ff;
  return `rgb(${Math.abs(r)}, ${Math.abs(g)}, ${Math.abs(b)})`;
}

export function CircularNodeGraph({
  width,
  height,
  nodes,
  edges,
  focusedNodeId,
  selectedNodeId,
  onNodeHover,
  onNodeSelect,
  nodeStats,
  resetViewToken,
}: CircularNodeGraphProps) {
  const svgRef = useRef<SVGSVGElement | null>(null);
  const [zoom, setZoom] = useState(1);
  const [pan, setPan] = useState({ x: 0, y: 0 });
  const [isDragging, setIsDragging] = useState(false);
  const dragStart = useRef({ x: 0, y: 0 });
  const panStart = useRef({ x: 0, y: 0 });

  const nodePositions = useMemo(
    () => new Map(nodes.map((node) => [node.id, node])),
    [nodes]
  );

  const connectedToFocus = useMemo(() => {
    if (!focusedNodeId) return new Set<string>();
    const related = new Set<string>([focusedNodeId]);
    for (const edge of edges) {
      if (edge.from === focusedNodeId) related.add(edge.to);
      if (edge.to === focusedNodeId) related.add(edge.from);
    }
    return related;
  }, [edges, focusedNodeId]);

  useEffect(() => {
    if (resetViewToken === undefined) return;
    setZoom(1);
    setPan({ x: 0, y: 0 });
  }, [resetViewToken]);

  function handleWheel(event: WheelEvent<SVGSVGElement>) {
    event.preventDefault();

    const nextZoom = Math.min(
      MAX_ZOOM,
      Math.max(MIN_ZOOM, zoom * (event.deltaY > 0 ? ZOOM_OUT_FACTOR : ZOOM_IN_FACTOR))
    );
    const svg = svgRef.current;
    if (!svg) {
      setZoom(nextZoom);
      return;
    }

    const rect = svg.getBoundingClientRect();
    const cursorX = event.clientX - rect.left;
    const cursorY = event.clientY - rect.top;
    const scaleFactor = nextZoom / zoom;

    setPan({
      x: cursorX - (cursorX - pan.x) * scaleFactor,
      y: cursorY - (cursorY - pan.y) * scaleFactor,
    });
    setZoom(nextZoom);
  }

  function handleMouseDown(event: MouseEvent<SVGSVGElement>) {
    setIsDragging(true);
    dragStart.current = { x: event.clientX, y: event.clientY };
    panStart.current = { ...pan };
  }

  function handleMouseMove(event: MouseEvent<SVGSVGElement>) {
    if (!isDragging) return;
    const dx = event.clientX - dragStart.current.x;
    const dy = event.clientY - dragStart.current.y;
    setPan({ x: panStart.current.x + dx, y: panStart.current.y + dy });
  }

  function handleMouseUp() {
    setIsDragging(false);
  }

  return (
    <svg
      ref={svgRef}
      viewBox={`0 0 ${width} ${height}`}
      preserveAspectRatio="xMidYMid meet"
      className="h-full w-full"
      role="img"
      aria-label="Circular node graph"
      onWheel={handleWheel}
      onMouseDown={handleMouseDown}
      onMouseMove={handleMouseMove}
      onMouseUp={handleMouseUp}
      onMouseLeave={() => {
        handleMouseUp();
        onNodeHover?.(null);
      }}
    >
      <defs>
        <marker
          id="graph-arrow"
          viewBox="0 0 8 8"
          refX="6"
          refY="4"
          markerWidth="6"
          markerHeight="6"
          orient="auto"
          markerUnits="strokeWidth"
        >
          <path d="M 0 0 L 8 4 L 0 8 z" fill="rgba(148, 163, 184, 0.65)" />
        </marker>
      </defs>
      {/* Circle guide for layout */}
      <circle
        cx={width / 2}
        cy={height / 2}
        r={Math.min(width, height) * 0.35}
        fill="none"
        stroke="rgba(148, 163, 184, 0.15)"
        strokeDasharray="4 6"
      />

      {/* Graph content that pans/zooms together */}
      <g transform={`translate(${pan.x} ${pan.y}) scale(${zoom})`}>
        {edges.map((edge) => {
          const from = nodePositions.get(edge.from);
          const to = nodePositions.get(edge.to);
          if (!from || !to) return null;
          const isFocused =
            focusedNodeId &&
            (edge.from === focusedNodeId || edge.to === focusedNodeId);
          const opacity = focusedNodeId ? (isFocused ? 0.85 : 0.15) : 0.35;
          const dx = to.x - from.x;
          const dy = to.y - from.y;
          const length = Math.hypot(dx, dy);
          if (length === 0) return null;
          const unitX = dx / length;
          const unitY = dy / length;
          const startOffset = getNodeRadius(selectedNodeId, edge.from);
          const endOffset = getNodeRadius(selectedNodeId, edge.to) + 2;
          const x1 = from.x + unitX * startOffset;
          const y1 = from.y + unitY * startOffset;
          const x2 = to.x - unitX * endOffset;
          const y2 = to.y - unitY * endOffset;

          return (
            <line
              key={`${edge.from}-${edge.to}`}
              x1={x1}
              y1={y1}
              x2={x2}
              y2={y2}
              stroke={edgeColor(edge)}
              opacity={opacity}
              strokeWidth={1.4}
              markerEnd="url(#graph-arrow)"
            />
          );
        })}

        {nodes.map((node) => (
          <g key={node.id}>
            {selectedNodeId === node.id && (
              <circle
                cx={node.x}
                cy={node.y}
                r={22}
                fill="none"
                stroke="rgba(99, 102, 241, 0.5)"
                strokeWidth={2}
              />
            )}
            <circle
              cx={node.x}
              cy={node.y}
              r={getNodeRadius(selectedNodeId, node.id)}
              fill="rgba(99, 102, 241, 0.9)"
              stroke="rgba(129, 140, 248, 0.9)"
              strokeWidth={1}
              className={onNodeSelect ? 'cursor-pointer' : undefined}
              style={{
                opacity:
                  focusedNodeId && !connectedToFocus.has(node.id) ? 0.2 : 1,
              }}
              onMouseEnter={() => onNodeHover?.(node.id)}
              onMouseLeave={() => onNodeHover?.(null)}
              onMouseDown={(event: MouseEvent<SVGCircleElement>) =>
                event.preventDefault()
              }
              onClick={() => onNodeSelect?.(node.id)}
            />
          </g>
        ))}
      </g>

      {/* Labels are in screen space so they don't scale with zoom */}
      {nodes.map((node) => {
        const screenX = pan.x + node.x * zoom;
        const screenY = pan.y + node.y * zoom;
        const isDimmed =
          focusedNodeId && !connectedToFocus.has(node.id) ? true : false;
        return (
          <text
            key={`${node.id}-label`}
            x={screenX}
            y={screenY - 22}
            textAnchor="middle"
            className="fill-slate-300 text-[12px]"
            style={{ opacity: isDimmed ? 0.2 : 1, pointerEvents: 'none' }}
          >
            {node.id}
          </text>
        );
      })}

      {/* Edge speed labels only for outgoing edges from focused node */}
      {focusedNodeId &&
        edges.map((edge) => {
          if (edge.from !== focusedNodeId) {
            return null;
          }
          const from = nodePositions.get(edge.from);
          const to = nodePositions.get(edge.to);
          if (!from || !to) return null;

          const midX = (from.x + to.x) / 2;
          const midY = (from.y + to.y) / 2;
          const screenX = pan.x + midX * zoom;
          const screenY = pan.y + midY * zoom;
          const label = `In ${formatRate(edge.inBytesPerSec)} B/s • Out ${formatRate(
            edge.outBytesPerSec
          )} B/s`;

          return (
            <text
              key={`${edge.from}-${edge.to}-label`}
              x={screenX}
              y={screenY - 10}
              textAnchor="middle"
              className="fill-slate-300 text-[11px]"
              style={{ pointerEvents: 'none' }}
            >
              {label}
            </text>
          );
        })}

      {/* Focused node info card */}
      {focusedNodeId && nodeStats?.has(focusedNodeId) && (() => {
        const node = nodePositions.get(focusedNodeId);
        const stats = nodeStats.get(focusedNodeId);
        if (!node || !stats) return null;

        const screenX = pan.x + node.x * zoom + 28;
        const screenY = pan.y + node.y * zoom - 10;
        return (
          <g>
            <rect
              x={screenX}
              y={screenY}
              width={180}
              height={48}
              rx={8}
              fill="rgba(15, 23, 42, 0.85)"
              stroke="rgba(148, 163, 184, 0.3)"
            />
            <text
              x={screenX + 10}
              y={screenY + 16}
              className="fill-slate-100 text-[11px]"
            >
              {focusedNodeId}
            </text>
            <text
              x={screenX + 10}
              y={screenY + 32}
              className="fill-slate-300 text-[10px]"
            >
              Down {formatRate(stats.downloadKbps)} B/s • Up{' '}
              {formatRate(stats.uploadKbps)} B/s
            </text>
            <text
              x={screenX + 10}
              y={screenY + 46}
              className="fill-slate-400 text-[9px]"
            >
              {stats.connections} connections
            </text>
          </g>
        );
      })()}
    </svg>
  );
}
