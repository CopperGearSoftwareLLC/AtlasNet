'use client';

import type { CSSProperties } from 'react';
import type { AuthorityLinkMode } from '../../lib/cartographTypes';
import type {
  MapProjectionMode,
  MapViewMode,
} from '../../lib/mapRenderer';

interface MapHudProps {
  showGnsConnections: boolean;
  showAuthorityEntities: boolean;
  authorityLinkMode: AuthorityLinkMode;
  showShardHoverDetails: boolean;
  onToggleGnsConnections: () => void;
  onToggleAuthorityEntities: () => void;
  onSetAuthorityLinkMode: (mode: AuthorityLinkMode) => void;
  onToggleShardHoverDetails: () => void;
  entityCount: number;
  shardCount: number;
  networkEdgeCount: number;
  claimedEntityCount: number;
  viewMode: MapViewMode;
  projectionMode: MapProjectionMode;
  onSetViewMode: (mode: MapViewMode) => void;
  onSetProjectionMode: (mode: MapProjectionMode) => void;
  interactionSensitivity: number;
  onSetInteractionSensitivity: (value: number) => void;
  playbackActive: boolean;
  onTakeSnapshot: () => void;
  onResumeLive: () => void;
  pollIntervalMs: number;
  minPollIntervalMs: number;
  maxPollIntervalMs: number;
  onSetPollIntervalMs: (value: number) => void;
}

const HUD_ACTIVE_BUTTON_BG = 'rgba(56, 189, 248, 0.3)';
const HUD_IDLE_BUTTON_BG = 'rgba(15, 23, 42, 0.65)';
const HUD_BUTTON_BASE_STYLE: CSSProperties = {
  padding: '4px 8px',
  borderRadius: 6,
  border: '1px solid rgba(148, 163, 184, 0.45)',
  color: '#e2e8f0',
};
const MIN_INTERACTION_SENSITIVITY = 0;
const INTERACTION_SENSITIVITY_SLIDER_HEADROOM = 10;

function hudButtonStyle(active = false): CSSProperties {
  return {
    ...HUD_BUTTON_BASE_STYLE,
    background: active ? HUD_ACTIVE_BUTTON_BG : HUD_IDLE_BUTTON_BG,
  };
}

function clamp(value: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, value));
}

function parseBoundedNumber(
  raw: string,
  min: number,
  max: number,
  fallback: number
): number {
  const parsed = Number(raw);
  if (!Number.isFinite(parsed)) {
    return fallback;
  }
  return clamp(parsed, min, max);
}

export function MapHud({
  claimedEntityCount,
  entityCount,
  interactionSensitivity,
  maxPollIntervalMs,
  minPollIntervalMs,
  networkEdgeCount,
  onSetInteractionSensitivity,
  onSetPollIntervalMs,
  onSetProjectionMode,
  onSetViewMode,
  onResumeLive,
  onSetAuthorityLinkMode,
  onTakeSnapshot,
  onToggleAuthorityEntities,
  onToggleGnsConnections,
  onToggleShardHoverDetails,
  authorityLinkMode,
  playbackActive,
  pollIntervalMs,
  projectionMode,
  shardCount,
  showAuthorityEntities,
  showGnsConnections,
  showShardHoverDetails,
  viewMode,
}: MapHudProps) {
  const maxInteractionSensitivity = Math.max(
    2,
    Math.ceil(interactionSensitivity + INTERACTION_SENSITIVITY_SLIDER_HEADROOM)
  );
  const pollDisabled = pollIntervalMs >= maxPollIntervalMs;
  const pollSupportsOneMsSpecialCase = minPollIntervalMs === 1;
  const pollSliderMin = pollSupportsOneMsSpecialCase ? 0 : minPollIntervalMs;
  const pollSliderStep = 5;
  const pollSliderValue =
    pollSupportsOneMsSpecialCase && pollIntervalMs <= minPollIntervalMs
      ? 0
      : pollIntervalMs;
  function handlePollIntervalChange(rawValue: string): void {
    const nextValue = parseBoundedNumber(
      rawValue,
      pollSliderMin,
      maxPollIntervalMs,
      pollSliderMin
    );
    if (pollSupportsOneMsSpecialCase && nextValue <= pollSliderMin) {
      onSetPollIntervalMs(minPollIntervalMs);
      return;
    }
    onSetPollIntervalMs(nextValue);
  }

  return (
    <div
      style={{
        flex: '0 0 auto',
        background: 'rgba(15, 23, 42, 0.92)',
        color: '#e2e8f0',
        borderTop: '1px solid rgba(148, 163, 184, 0.35)',
        padding: '10px 14px 12px',
        display: 'flex',
        gap: 12,
        alignItems: 'center',
        flexWrap: 'wrap',
        fontSize: 13,
        backdropFilter: 'blur(6px)',
      }}
    >
      <label style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
        <input
          type="checkbox"
          checked={showGnsConnections}
          onChange={() => onToggleGnsConnections()}
        />
        GNS connections
      </label>
      <label style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
        <input
          type="checkbox"
          checked={showAuthorityEntities}
          onChange={() => onToggleAuthorityEntities()}
        />
        entities
      </label>
      <div
        style={{
          display: 'inline-flex',
          alignItems: 'center',
          gap: 4,
          opacity: showAuthorityEntities ? 1 : 0.65,
        }}
      >
        <span style={{ fontSize: 12, opacity: 0.9 }}>links:</span>
        <button
          type="button"
          style={hudButtonStyle(authorityLinkMode === 'owner')}
          onClick={() => onSetAuthorityLinkMode('owner')}
          disabled={!showAuthorityEntities}
        >
          owner
        </button>
        <button
          type="button"
          style={hudButtonStyle(authorityLinkMode === 'handoff')}
          onClick={() => onSetAuthorityLinkMode('handoff')}
          disabled={!showAuthorityEntities}
        >
          handoff
        </button>
      </div>
      <label style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
        <input
          type="checkbox"
          checked={showShardHoverDetails}
          onChange={() => onToggleShardHoverDetails()}
        />
        shard hover details
      </label>
      <button
        type="button"
        style={hudButtonStyle(playbackActive)}
        onClick={onTakeSnapshot}
        title="Snapshot and pause last 30 seconds"
      >
        snapshot 30s
      </button>
      {playbackActive && (
        <button
          type="button"
          style={hudButtonStyle(false)}
          onClick={onResumeLive}
          title="Exit snapshot playback"
        >
          resume live
        </button>
      )}
      <span style={{ opacity: 0.8 }}>
        entities: {entityCount} | shards: {shardCount}
      </span>
      <span style={{ opacity: 0.8 }}>
        connections: {networkEdgeCount} | claimed entities: {claimedEntityCount}
      </span>

      <div style={{ display: 'inline-flex', alignItems: 'center', gap: 8 }}>
        <div
          style={{
            display: 'inline-flex',
            alignItems: 'center',
            gap: 6,
            padding: '4px',
            borderRadius: 8,
            border: '1px solid rgba(148, 163, 184, 0.45)',
            background: 'rgba(2, 6, 23, 0.5)',
          }}
        >
          <button
            type="button"
            onClick={() => onSetViewMode('2d')}
            style={hudButtonStyle(viewMode === '2d')}
            title="2D mode: drag to pan, mouse wheel to zoom"
          >
            2D
          </button>
          <button
            type="button"
            onClick={() => {
              onSetViewMode('3d');
              onSetProjectionMode('orthographic');
            }}
            style={hudButtonStyle(
              viewMode === '3d' && projectionMode === 'orthographic'
            )}
            title="3D Orthographic mode"
          >
            3D Ortho
          </button>
          <button
            type="button"
            onClick={() => {
              onSetViewMode('3d');
              onSetProjectionMode('perspective');
            }}
            style={hudButtonStyle(viewMode === '3d' && projectionMode === 'perspective')}
            title="3D Perspective mode"
          >
            3D Persp
          </button>
        </div>
      </div>

      <span style={{ opacity: 0.85, fontSize: 12 }}>
        {viewMode === '2d'
          ? '2D controls: LMB pan, wheel zoom, WASD map X/Y, F frame'
          : '3D controls: RMB orbit, LMB pan, wheel zoom, WASD camera-relative, F frame'}
      </span>

      <label
        style={{
          display: 'flex',
          gap: 8,
          alignItems: 'center',
          minWidth: 220,
        }}
      >
        sensitivity
        <input
          type="range"
          min={MIN_INTERACTION_SENSITIVITY}
          max={maxInteractionSensitivity}
          step={0.1}
          value={interactionSensitivity}
          onChange={(event) =>
            onSetInteractionSensitivity(
              parseBoundedNumber(
                event.target.value,
                MIN_INTERACTION_SENSITIVITY,
                maxInteractionSensitivity,
                MIN_INTERACTION_SENSITIVITY
              )
            )
          }
        />
        {interactionSensitivity.toFixed(1)}
      </label>

      <label
        style={{
          display: 'flex',
          gap: 8,
          alignItems: 'center',
          minWidth: 220,
        }}
      >
        <input
          type="range"
          min={pollSliderMin}
          max={maxPollIntervalMs}
          step={pollSliderStep}
          value={pollSliderValue}
          onChange={(event) => handlePollIntervalChange(event.target.value)}
        />
        poll: {pollDisabled ? 'off' : `${pollIntervalMs}ms`}
      </label>
    </div>
  );
}
