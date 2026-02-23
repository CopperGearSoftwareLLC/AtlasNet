'use client';

import { useEffect } from 'react';
import type { ShardTelemetry } from '../lib/cartographTypes';

function Metric({
  label,
  value,
  mono = true,
}: {
  label: string;
  value: string | number;
  mono?: boolean;
}) {
  return (
    <div>
      <div className="text-xs text-slate-400">{label}</div>
      <div className={mono ? 'font-mono text-sm text-slate-200' : 'text-sm'}>
        {value}
      </div>
    </div>
  );
}

export function TelemetryPanel({
  shard,
  onClose,
}: {
  shard: ShardTelemetry;
  onClose: () => void;
}) {
  // ✅ ESC key support (minimal, safe)
  useEffect(() => {
    function onKeyDown(e: KeyboardEvent) {
      if (e.key === 'Escape') {
        onClose();
      }
    }

    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [onClose]);

  return (
    // ✅ Fullscreen overlay
    <div
      className="fixed inset-0 z-50 bg-black/40"
      onClick={onClose} // click outside closes
    >
      {/* ✅ Panel */}
      <aside
        className="absolute right-0 top-0 h-full w-[85%] bg-slate-950 border-l border-slate-800 overflow-y-auto"
        onClick={(e) => e.stopPropagation()} // prevent close when interacting
      >
        <div className="space-y-6 p-6">
          {/* Header */}
          <div className="flex items-center justify-between">
            <div>
              <h2 className="text-xl font-semibold text-slate-100">
                Shard {shard.shardId}
              </h2>
              <p className="text-sm text-slate-400">
                Live network telemetry
              </p>
            </div>

            <button
              onClick={onClose}
              className="rounded-xl border border-slate-700 px-3 py-1 text-sm text-slate-300 hover:bg-slate-800"
            >
              Close
            </button>
          </div>

          {/* Summary */}
          <div className="grid grid-cols-3 gap-4 rounded-2xl bg-slate-900/60 border border-slate-800 p-4">
            <Metric label="Download" value={`${shard.downloadKbps.toFixed(1)} Bytes/s`} />
            <Metric label="Upload" value={`${shard.uploadKbps.toFixed(1)} Bytes/s`} />
            <Metric label="Connections" value={shard.connections.length} />
          </div>

          {/* Connections */}
          <div className="space-y-4">
            <h3 className="text-sm font-medium text-slate-300">
              Connections
            </h3>

            {shard.connections.map((c, idx) => (
              <div
                key={idx}
                className="rounded-2xl bg-slate-900/80 border border-slate-800 p-4 space-y-3"
              >
                {/* Identity row */}
                <div className="flex items-center justify-between">
                  <div>
                    <div className="text-sm font-medium text-slate-100">
                      {c.IdentityId} → {c.targetId}
                    </div>
                  </div>
                </div>

                {/* Metrics */}
                <div className="grid grid-cols-4 gap-4">
                  {/* Latency / quality */}
                  <Metric label="Ping (ms)" value={c.pingMs} />
                  <Metric label="Local Quality" value={c.qualityLocal} />
                  <Metric label="Remote Quality" value={c.qualityRemote} />
                  <Metric label="Queue (µs)" value={c.queueTimeUsec} />

                  {/* Throughput */}
                  <Metric label="In Bytes/s" value={c.inBytesPerSec} />
                  <Metric label="Out Bytes/s" value={c.outBytesPerSec} />
                  <Metric label="In Packets/s" value={c.inPacketsPerSec} />

                  {/* Reliability */}
                  <Metric label="Pending Reliable Bytes" value={c.pendingReliableBytes} />
                  <Metric label="Pending Unreliable Bytes" value={c.pendingUnreliableBytes} />
                  <Metric label="Unacked Reliable Bytes" value={c.sentUnackedReliableBytes} />
                  <Metric label="State" value={c.state} />
                </div>
              </div>
            ))}

            {shard.connections.length === 0 && (
              <div className="text-sm text-slate-500 italic">
                No active connections.
              </div>
            )}
          </div>
        </div>
      </aside>
    </div>
  );
}
