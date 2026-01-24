'use client';

import type { ShardTelemetry } from '../lib/networkTelemetryTypes';

const CONNECTION_FIELDS = [
  { key: 'connectionId', label: 'Conn ID' },
  { key: 'sourceShard', label: 'Src' },
  { key: 'destShard', label: 'Dst' },
  { key: 'rttMs', label: 'RTT (ms)', align: 'right' },
  { key: 'packetLossPct', label: 'Loss %', align: 'right' },
  { key: 'sendKbps', label: 'Send KB/s', align: 'right' },
  { key: 'recvKbps', label: 'Recv KB/s', align: 'right' },
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
        width: '40%',
        height: '100%',
        background: '#111',
        color: '#eee',
        borderLeft: '1px solid #333',
        padding: 12,
        overflow: 'auto',
        zIndex: 1000,
      }}
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
        <div>downloadKbps: {shard.downloadKbps}</div>
        <div>uploadKbps: {shard.uploadKbps}</div>
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
