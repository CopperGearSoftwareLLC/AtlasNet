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
}: ShardTelemetryRowProps) {
  return (
    <tr>
      <td style={{ padding: 8 }}>{shardId}</td>

      <td style={{ padding: 8, textAlign: 'right', whiteSpace: 'nowrap' }}>
        {downloadKbps.toFixed(1)} KB/s
      </td>

      <td style={{ padding: 8 }}>
        <LineGraphCanvas values={downloadHistory} width={220} height={60} />
      </td>

      <td style={{ padding: 8, textAlign: 'right', whiteSpace: 'nowrap' }}>
        {uploadKbps.toFixed(1)} KB/s
      </td>

      <td style={{ padding: 8 }}>
        <LineGraphCanvas values={uploadHistory} width={220} height={60} />
      </td>
    </tr>
  );
}
