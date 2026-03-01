'use client';

interface MapPlaybackBarProps {
  visible: boolean;
  startMs: number;
  endMs: number;
  cursorMs: number;
  paused: boolean;
  direction: 1 | -1;
  onPlayForward: () => void;
  onPlayReverse: () => void;
  onTogglePause: () => void;
  onSeek: (nextCursorMs: number) => void;
  onResumeLive: () => void;
}

function clamp(value: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, value));
}

function formatSeconds(valueMs: number): string {
  if (!Number.isFinite(valueMs)) {
    return '0.0s';
  }
  return `${(valueMs / 1000).toFixed(1)}s`;
}

export function MapPlaybackBar({
  cursorMs,
  direction,
  endMs,
  onPlayForward,
  onPlayReverse,
  onResumeLive,
  onSeek,
  onTogglePause,
  paused,
  startMs,
  visible,
}: MapPlaybackBarProps) {
  if (!visible) {
    return null;
  }

  const durationMs = Math.max(1, endMs - startMs);
  const clampedCursorMs = clamp(cursorMs, startMs, endMs);
  const relativeMs = clampedCursorMs - startMs;

  return (
    <div
      style={{
        position: 'absolute',
        left: 14,
        right: 14,
        bottom: 10,
        zIndex: 25,
        borderRadius: 10,
        border: '1px solid rgba(148, 163, 184, 0.45)',
        background: 'rgba(2, 6, 23, 0.9)',
        color: '#e2e8f0',
        backdropFilter: 'blur(8px)',
        padding: '8px 10px',
        display: 'flex',
        alignItems: 'center',
        gap: 10,
      }}
    >
      <button
        type="button"
        onClick={onPlayReverse}
        title="Play in reverse"
        style={{
          border: '1px solid rgba(148, 163, 184, 0.45)',
          borderRadius: 6,
          padding: '4px 9px',
          background:
            !paused && direction < 0
              ? 'rgba(56, 189, 248, 0.34)'
              : 'rgba(15, 23, 42, 0.72)',
          color: '#e2e8f0',
        }}
      >
        {'<'}
      </button>

      <button
        type="button"
        onClick={onTogglePause}
        title={paused ? 'Resume playback' : 'Pause playback'}
        style={{
          border: '1px solid rgba(148, 163, 184, 0.45)',
          borderRadius: 6,
          padding: '4px 10px',
          background: paused
            ? 'rgba(15, 23, 42, 0.72)'
            : 'rgba(56, 189, 248, 0.34)',
          color: '#e2e8f0',
          minWidth: 58,
        }}
      >
        {paused ? 'play' : 'pause'}
      </button>

      <button
        type="button"
        onClick={onPlayForward}
        title="Play forward"
        style={{
          border: '1px solid rgba(148, 163, 184, 0.45)',
          borderRadius: 6,
          padding: '4px 9px',
          background:
            !paused && direction > 0
              ? 'rgba(56, 189, 248, 0.34)'
              : 'rgba(15, 23, 42, 0.72)',
          color: '#e2e8f0',
        }}
      >
        {'>'}
      </button>

      <span
        style={{
          fontSize: 11,
          opacity: 0.85,
          minWidth: 76,
          textAlign: 'right',
          fontVariantNumeric: 'tabular-nums',
        }}
      >
        {formatSeconds(relativeMs)}
      </span>

      <input
        type="range"
        min={0}
        max={durationMs}
        step={50}
        value={relativeMs}
        onChange={(event) =>
          onSeek(
            startMs + clamp(Number(event.target.value), 0, durationMs)
          )
        }
        style={{ flex: '1 1 auto' }}
      />

      <span
        style={{
          fontSize: 11,
          opacity: 0.65,
          minWidth: 76,
          fontVariantNumeric: 'tabular-nums',
        }}
      >
        {formatSeconds(durationMs)}
      </span>

      <button
        type="button"
        onClick={onResumeLive}
        title="Exit snapshot playback"
        style={{
          border: '1px solid rgba(148, 163, 184, 0.45)',
          borderRadius: 6,
          padding: '4px 9px',
          background: 'rgba(15, 23, 42, 0.72)',
          color: '#e2e8f0',
        }}
      >
        live
      </button>
    </div>
  );
}
