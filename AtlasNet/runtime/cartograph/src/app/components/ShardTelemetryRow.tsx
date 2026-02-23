'use client';

import React from 'react';
import { LineGraphCanvas } from './LineGraphCanvas';

export type ShardTelemetryRowProps = {
  shardId: string;

  /** Latest values (for table cells) */
  downloadKbps: number;
  uploadKbps: number;

  /** Rolling histories (for graphs) */
  downloadHistory: number[];
  uploadHistory: number[];

  /**
   * Called when the user wants to open the detailed telemetry side panel
   * for this shard.
   */
  onOpenTelemetry: (shardId: string) => void;
};

/**
 * Displays one shard's telemetry: shard id, current rates, and mini line graphs.
 */
export function ShardTelemetryRow({
  shardId,
  downloadKbps,
  uploadKbps,
  downloadHistory,
  uploadHistory,
  onOpenTelemetry,
}: ShardTelemetryRowProps) {
  return (
    <tr>
      <td style={{ padding: 8 }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
          <span>{shardId}</span>
          <button
            type="button"
            onClick={() => onOpenTelemetry(shardId)}
            style={{
              padding: '2px 8px',
              fontSize: 12,
              cursor: 'pointer',
            }}
            aria-label={`Open detailed telemetry for ${shardId}`}
            title="Open detailed telemetry"
          >
            Details
          </button>
        </div>
      </td>

      <td style={{ padding: 8, textAlign: 'right', whiteSpace: 'nowrap' }}>
        {downloadKbps.toFixed(1)} Bytes/s
      </td>

      <td style={{ padding: 8 }}>
        <LineGraphCanvas values={downloadHistory} width={220} height={60} />
      </td>

      <td style={{ padding: 8, textAlign: 'right', whiteSpace: 'nowrap' }}>
        {uploadKbps.toFixed(1)} Bytes/s
      </td>

      <td style={{ padding: 8 }}>
        <LineGraphCanvas values={uploadHistory} width={220} height={60} />
      </td>
    </tr>
  );
}
