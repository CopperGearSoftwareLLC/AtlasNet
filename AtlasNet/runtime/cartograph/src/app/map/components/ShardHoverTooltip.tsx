'use client';

import { formatRate } from '../../lib/mapData';

interface HoverAnchor {
  x: number;
  y: number;
}

interface ShardHoverTooltipProps {
  hoveredShardId: string | null;
  hoveredShardAnchor: HoverAnchor | null;
  downloadBytesPerSec: number;
  uploadBytesPerSec: number;
  outgoingConnectionCount: number;
  workerNodeName: string | null;
}

export function ShardHoverTooltip({
  downloadBytesPerSec,
  hoveredShardAnchor,
  hoveredShardId,
  outgoingConnectionCount,
  uploadBytesPerSec,
  workerNodeName,
}: ShardHoverTooltipProps) {
  if (!hoveredShardId || !hoveredShardAnchor) {
    return null;
  }

  return (
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
        Down {formatRate(downloadBytesPerSec)} B/s {' • '}Up {formatRate(uploadBytesPerSec)} B/s
      </div>
      <div style={{ color: '#cbd5e1', marginBottom: 4 }}>
        Worker node: {workerNodeName || 'unknown'}
      </div>
      <div style={{ color: '#94a3b8' }}>{outgoingConnectionCount} outgoing connections</div>
    </div>
  );
}
