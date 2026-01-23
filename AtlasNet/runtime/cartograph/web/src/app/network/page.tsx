'use client';

import { useEffect, useState } from 'react';
import type { ShardTelemetry } from '../lib/networkTelemetryTypes';
import { ShardTelemetryRow } from '../components/ShardTelemetryRow';

type ShardState = {
  shardId: string;
  downloadKbps: number;
  uploadKbps: number;
  downloadHistory: number[];
  uploadHistory: number[];
};

const HISTORY_LEN = 60;

/** Push a value into a rolling buffer */
function pushRolling(prev: number[], value: number): number[] {
  if (prev.length >= HISTORY_LEN) {
    return [...prev.slice(1), value];
  }
  return [...prev, value];
}

export default function NetworkTelemetryPage() {
  const [shards, setShards] = useState<ShardState[]>([]);

  useEffect(() => {
    let alive = true;

    async function poll() {
      try {
        const res = await fetch('/api/networktelemetry', {
          cache: 'no-store',
        });

        if (!res.ok) {
          console.error('network telemetry fetch failed');
          return;
        }

        const data = (await res.json()) as ShardTelemetry[];

        if (!alive) return;

        setShards(prev => {
          const prevById = new Map(prev.map(s => [s.shardId, s]));

          return data.map(t => {
            const old = prevById.get(t.shardId);

            return {
              shardId: t.shardId,
              downloadKbps: t.downloadKbps,
              uploadKbps: t.uploadKbps,
              downloadHistory: pushRolling(
                old?.downloadHistory ?? [],
                t.downloadKbps
              ),
              uploadHistory: pushRolling(
                old?.uploadHistory ?? [],
                t.uploadKbps
              ),
            };
          });
        });
      } catch (err) {
        console.error(err);
      }
    }

    // initial + interval
    poll();
    const id = setInterval(poll, 1000);

    return () => {
      alive = false;
      clearInterval(id);
    };
  }, []);

  return (
    <div style={{ padding: 16 }}>
      <h2>Network Telemetry</h2>

      <table
        style={{
          width: '100%',
          borderCollapse: 'collapse',
        }}
      >
        <thead>
          <tr style={{ borderBottom: '1px solid #ccc' }}>
            <th align="left">Shard</th>
            <th align="right">Download</th>
            <th>Download Graph</th>
            <th align="right">Upload</th>
            <th>Upload Graph</th>
          </tr>
        </thead>

        <tbody>
          {shards.map(s => (
            <ShardTelemetryRow
              key={s.shardId}
              shardId={s.shardId}
              downloadKbps={s.downloadKbps}
              uploadKbps={s.uploadKbps}
              downloadHistory={s.downloadHistory}
              uploadHistory={s.uploadHistory}
            />
          ))}
        </tbody>
      </table>
    </div>
  );
}
