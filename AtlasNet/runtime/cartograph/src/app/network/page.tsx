'use client';

import { useEffect, useState, useMemo } from 'react';
import { ServerBoundsMinimapSection } from '../components/ServerBoundsMinimapSection';
import { CircularNodeGraphPanel } from '../components/CircularNodeGraphPanel';
import { ShardTelemetryRow } from '../components/ShardTelemetryRow';
import { TelemetryPanel } from '../components/TelemetryPanel';
import {
  useAuthorityEntities,
  useHeuristicShapes,
  useNetworkTelemetry,
} from '../lib/hooks/useTelemetryFeeds';
import { useServerBoundsMinimapData } from '../lib/hooks/useServerBoundsMinimapData';

const ENABLE_NETWORK_TELEMETRY = true;
const DEFAULT_POLL_INTERVAL_MS = 200;
const MIN_POLL_INTERVAL_MS = 50;
const MAX_POLL_INTERVAL_MS = 1000;
const POLL_DISABLED_AT_MS = MAX_POLL_INTERVAL_MS;
const SERVER_BOUNDS_POLL_INTERVAL_MS = 1000;

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
  const [selectedShardId, setSelectedShardId] = useState<string | null>(null);
  const [pollIntervalMs, setPollIntervalMs] = useState(DEFAULT_POLL_INTERVAL_MS);
  const [showServerBoundsMinimap, setShowServerBoundsMinimap] = useState(true);
  const telemetryPollIntervalMs =
    pollIntervalMs >= POLL_DISABLED_AT_MS ? 0 : pollIntervalMs;
  const latestTelemetry = useNetworkTelemetry({
    intervalMs: telemetryPollIntervalMs,
    enabled: ENABLE_NETWORK_TELEMETRY,
    resetOnException: false,
    resetOnHttpError: false,
    onHttpError: () => {
      console.error('network telemetry fetch failed');
    },
    onException: (err) => {
      console.error(err);
    },
  });
  const heuristicShapes = useHeuristicShapes({
    intervalMs: SERVER_BOUNDS_POLL_INTERVAL_MS,
    enabled: showServerBoundsMinimap,
    resetOnException: true,
    resetOnHttpError: false,
  });
  const authorityEntities = useAuthorityEntities({
    intervalMs: SERVER_BOUNDS_POLL_INTERVAL_MS,
    enabled: showServerBoundsMinimap,
    resetOnException: true,
    resetOnHttpError: false,
  });
  const {
    shardSummaries,
    shardBoundsByIdWithNetworkFallback,
    claimedBoundShardIds,
  } = useServerBoundsMinimapData({
    heuristicShapes,
    authorityEntities,
    networkTelemetry: latestTelemetry,
  });

  const latestById = useMemo(() => {
    return new Map(latestTelemetry.map(t => [t.shardId, t]));
  }, [latestTelemetry]);

  const selectedShard = selectedShardId ? latestById.get(selectedShardId) : null;

  useEffect(() => {
    setShards((prev) => {
      const prevById = new Map(prev.map((s) => [s.shardId, s]));

      return latestTelemetry.map((t) => {
        const old = prevById.get(t.shardId);
        return {
          shardId: t.shardId,
          downloadKbps: t.downloadKbps,
          uploadKbps: t.uploadKbps,
          downloadHistory: pushRolling(old?.downloadHistory ?? [], t.downloadKbps),
          uploadHistory: pushRolling(old?.uploadHistory ?? [], t.uploadKbps),
        };
      });
    });
  }, [latestTelemetry]);

  return (
    <div style={{ padding: 16 }}>
      <h2>Network Telemetry</h2>
      <div
        style={{
          margin: '8px 0 14px',
          display: 'inline-flex',
          alignItems: 'center',
          gap: 10,
          padding: '8px 10px',
          borderRadius: 10,
          border: '1px solid rgba(148, 163, 184, 0.45)',
          background: 'rgba(15, 23, 42, 0.82)',
          color: '#e2e8f0',
          fontSize: 13,
        }}
      >
        <span>
          poll: {pollIntervalMs >= POLL_DISABLED_AT_MS ? 'off' : `${pollIntervalMs}ms`}
        </span>
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
        <label
          style={{
            display: 'inline-flex',
            alignItems: 'center',
            gap: 6,
            marginLeft: 6,
            cursor: 'pointer',
          }}
        >
          <input
            type="checkbox"
            checked={showServerBoundsMinimap}
            onChange={(event) => setShowServerBoundsMinimap(event.target.checked)}
          />
          <span>server bounds minimap</span>
        </label>
      </div>
      <div style={{ margin: '12px 0 16px' }}>
        <CircularNodeGraphPanel telemetry={latestTelemetry} />
      </div>

      <table
        style={{
          width: '100%',
          borderCollapse: 'collapse',
        }}
      >
        <thead>
          <tr style={{ borderBottom: '1px solid #ccc' }}>
            <th align="left">Shard</th>
            <th align="right">Download Avg</th>
            <th>Download Graph</th>
            <th align="right">Upload Avg</th>
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
              onOpenTelemetry={setSelectedShardId}
            />
          ))}
        </tbody>
      </table>
      {selectedShard && (
        <TelemetryPanel shard={selectedShard} onClose={() => setSelectedShardId(null)} />
      )}

      {showServerBoundsMinimap && (
        <div style={{ marginTop: 16 }}>
          <ServerBoundsMinimapSection
            shardSummaries={shardSummaries}
            boundsByShardId={shardBoundsByIdWithNetworkFallback}
            claimedBoundShardIds={claimedBoundShardIds}
            title="Server Bounds Minimap"
            emptyMessage="Waiting for shard telemetry and map bounds..."
          />
        </div>
      )}
    </div>
  );
}
