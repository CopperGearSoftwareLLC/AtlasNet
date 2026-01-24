'use client';

import type { ShardTelemetry } from '../lib/networkTelemetryTypes';

const CONNECTION_FIELDS = [
  { key: 'IdentityId', label: 'IdentityId' },
  { key: 'targetId', label: 'targetId' },
  { key: 'pingMs', label: 'ping (ms)', align: 'right' },
  { key: 'inBytesPerSec', label: 'In KB/s', align: 'right' },
  { key: 'outBytesPerSec', label: 'Out KB/s', align: 'right' },
  { key: 'inPacketsPerSec', label: 'In Packets/s', align: 'right' },
  { key: 'pendingReliableBytes', label: 'Pending Reliable Bytes', align: 'right' },
  { key: 'pendingUnreliableBytes', label: 'Pending Unreliable Bytes', align: 'right' },
  { key: 'sentUnackedReliableBytes', label: 'Sent Unacked Reliable Bytes', align: 'right' },
  { key: 'queueTimeUsec', label: 'Queue Time (us)', align: 'right' },
  { key: 'qualityLocal', label: 'Local Quality', align: 'right' },
  { key: 'qualityRemote', label: 'Remote Quality', align: 'right' },
  { key: 'state', label: 'State' },
] as const;

function getMaxCols(rows: string[][]): number {
  let max = 0;
  for (const r of rows) max = Math.max(max, r.length);
  return max;
}

export function TelemetryPanel({
  shard,
  onClose,
}: {
  shard: ShardTelemetry;
  onClose: () => void;
}) {
  const rows = shard.connections ?? [];
  const colCount = getMaxCols(rows);
  const headers = Array.from({ length: colCount }, (_, i) => `F${i}`);

  return (
    <aside
      style={{
        position: 'fixed',
        right: 0,
        top: 0,
        width: '85%',
        height: '100%',
        background: '#111',
        color: '#eee',
        borderLeft: '1px solid #333',
        padding: 12,
        overflow: 'auto',
        zIndex: 1000,
      }}
      //refactor to have connection table display as rows, not columns
    >
      <header style={{ display: 'flex', justifyContent: 'space-between' }}>
        <h3>Shard {shard.shardId}</h3>
        <button onClick={onClose}>Close</button>
      </header>

      <table>
        <thead>
          <tr>
            {CONNECTION_FIELDS.map(f => (
              <th key={f.key}>{f.label}</th>
            ))}
          </tr>
        </thead>

        <tbody>
          {shard.connections.map((c, i) => (
            <tr key={i}>
              {CONNECTION_FIELDS.map(f => (
                <td key={f.key} style={{ textAlign: f.align ?? 'left' }}>
                  {c[f.key]}
                </td>
              ))}
            </tr>
          ))}
        </tbody>
      </table>
      <div style={{ marginBottom: 8, fontFamily: 'monospace', fontSize: 12 }}>
        <div>rows: {rows.length}</div>
      </div>

      <table
        style={{
          width: '100%',
          borderCollapse: 'collapse',
          fontSize: 12,
          fontFamily: 'monospace',
        }}
      >
        <thead>
          <tr>
            {headers.map(h => (
              <th key={h} style={{ borderBottom: '1px solid #444', textAlign: 'left' }}>
                {h}
              </th>
            ))}
          </tr>
        </thead>

        <tbody>
          {rows.map((r, i) => (
            <tr key={i}>
              {headers.map((_, c) => (
                <td
                  key={c}
                  style={{
                    padding: '2px 4px',
                    textAlign: 'left',
                    borderBottom: '1px solid #222',
                    whiteSpace: 'nowrap',
                  }}
                >
                  {r[c] ?? ''}
                </td>
              ))}
            </tr>
          ))}
        </tbody>
      </table>
    </aside>
  );
}
