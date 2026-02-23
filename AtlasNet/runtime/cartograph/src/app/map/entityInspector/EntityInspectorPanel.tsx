'use client';

import { useEffect, useRef, useState } from 'react';
import type { MouseEvent as ReactMouseEvent } from 'react';
import type { AuthorityEntityTelemetry } from '../../lib/cartographTypes';
import { DatabaseExplorerInspector } from '../../database/components/DatabaseExplorerInspector';
import { useEntityDatabaseDetails } from './useEntityDatabaseDetails';

const ENTITY_INSPECTOR_DESKTOP_GRID_COLUMNS_CLASS =
  'lg:grid-cols-[minmax(0,1fr)_minmax(0,2fr)]';
const PANEL_MIN_WIDTH_PX = 520;

interface EntityInspectorPanelProps {
  selectedEntities: AuthorityEntityTelemetry[];
  activeEntityId: string | null;
  hoveredEntityId: string | null;
  pollIntervalMs: number;
  minPollIntervalMs: number;
  maxPollIntervalMs: number;
  pollDisabledAtMs: number;
  onSetPollIntervalMs: (value: number) => void;
  onSelectEntity: (entityId: string) => void;
  onHoverEntity: (entityId: string | null) => void;
  onClearSelection: () => void;
}

function formatNumber(value: number, digits = 2): string {
  if (!Number.isFinite(value)) {
    return '0';
  }
  return value.toFixed(digits);
}

export function EntityInspectorPanel({
  activeEntityId,
  hoveredEntityId,
  maxPollIntervalMs,
  minPollIntervalMs,
  onSetPollIntervalMs,
  pollIntervalMs,
  pollDisabledAtMs,
  onClearSelection,
  onHoverEntity,
  onSelectEntity,
  selectedEntities,
}: EntityInspectorPanelProps) {
  const panelRef = useRef<HTMLDivElement | null>(null);
  const resizeSessionRef = useRef<{
    startX: number;
    startWidth: number;
  } | null>(null);
  const [panelWidthPx, setPanelWidthPx] = useState<number | null>(null);

  function getMaxPanelWidthPx(): number {
    const parent = panelRef.current?.parentElement;
    if (!parent) {
      return Number.POSITIVE_INFINITY;
    }
    return Math.max(PANEL_MIN_WIDTH_PX, parent.clientWidth - 24);
  }

  useEffect(() => {
    const handleMouseMove = (event: MouseEvent) => {
      const session = resizeSessionRef.current;
      if (!session) {
        return;
      }

      const dragDelta = session.startX - event.clientX;
      const nextWidth = session.startWidth + dragDelta;
      const clampedWidth = Math.min(
        getMaxPanelWidthPx(),
        Math.max(PANEL_MIN_WIDTH_PX, nextWidth)
      );
      setPanelWidthPx(clampedWidth);
    };

    const handleMouseUp = () => {
      if (!resizeSessionRef.current) {
        return;
      }
      resizeSessionRef.current = null;
      document.body.style.cursor = '';
      document.body.style.userSelect = '';
    };

    const clampWidthToContainer = () => {
      setPanelWidthPx((previous) => {
        if (previous == null) {
          return previous;
        }
        return Math.min(
          getMaxPanelWidthPx(),
          Math.max(PANEL_MIN_WIDTH_PX, previous)
        );
      });
    };

    window.addEventListener('mousemove', handleMouseMove);
    window.addEventListener('mouseup', handleMouseUp);
    window.addEventListener('resize', clampWidthToContainer);
    clampWidthToContainer();

    let observer: ResizeObserver | null = null;
    const parent = panelRef.current?.parentElement;
    if (parent && typeof ResizeObserver !== 'undefined') {
      observer = new ResizeObserver(clampWidthToContainer);
      observer.observe(parent);
    }

    return () => {
      document.body.style.cursor = '';
      document.body.style.userSelect = '';
      window.removeEventListener('mousemove', handleMouseMove);
      window.removeEventListener('mouseup', handleMouseUp);
      window.removeEventListener('resize', clampWidthToContainer);
      observer?.disconnect();
    };
  }, []);

  function startResizeDrag(
    event: ReactMouseEvent<HTMLDivElement>
  ): void {
    if (event.button !== 0 || !panelRef.current) {
      return;
    }

    event.preventDefault();
    event.stopPropagation();
    resizeSessionRef.current = {
      startX: event.clientX,
      startWidth: panelRef.current.getBoundingClientRect().width,
    };
    document.body.style.cursor = 'ew-resize';
    document.body.style.userSelect = 'none';
  }

  const activeEntity =
    selectedEntities.find((entity) => entity.entityId === activeEntityId) ?? null;
  const visibleEntities = activeEntity ? [activeEntity] : selectedEntities;
  const details = useEntityDatabaseDetails(activeEntity, pollIntervalMs);

  if (selectedEntities.length === 0) {
    return null;
  }

  return (
    <div
      ref={panelRef}
      style={{
        position: 'absolute',
        top: 12,
        right: 12,
        bottom: 12,
        width: panelWidthPx == null ? '50%' : `${panelWidthPx}px`,
        minWidth: PANEL_MIN_WIDTH_PX,
        maxWidth: 'calc(100% - 24px)',
        display: 'flex',
        flexDirection: 'column',
        borderRadius: 10,
        border: '1px solid rgba(148, 163, 184, 0.35)',
        background: 'rgba(2, 6, 23, 0.9)',
        color: '#e2e8f0',
        backdropFilter: 'blur(4px)',
        pointerEvents: 'auto',
        overflow: 'hidden',
        zIndex: 30,
      }}
    >
      <div
        title="Drag to resize panel. Double-click to reset width."
        onMouseDown={startResizeDrag}
        onDoubleClick={() => setPanelWidthPx(null)}
        style={{
          position: 'absolute',
          left: 0,
          top: 0,
          bottom: 0,
          width: 10,
          cursor: 'ew-resize',
          zIndex: 10,
          background: 'rgba(148, 163, 184, 0.12)',
        }}
      />

      <div
        style={{
          padding: '10px 12px',
          borderBottom: '1px solid rgba(148, 163, 184, 0.25)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          gap: 12,
        }}
      >
        <div>
          <div style={{ fontSize: 12, opacity: 0.75 }}>Entity Inspector</div>
          <div style={{ fontSize: 14, fontWeight: 600 }}>
            {selectedEntities.length} selected
          </div>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
          <label
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: 8,
              fontSize: 11,
              color: '#cbd5e1',
              minWidth: 210,
            }}
          >
            detail poll:
            <input
              type="range"
              min={minPollIntervalMs}
              max={maxPollIntervalMs}
              step={250}
              value={pollIntervalMs}
              onChange={(event) =>
                onSetPollIntervalMs(
                  Math.max(
                    minPollIntervalMs,
                    Math.min(maxPollIntervalMs, Number(event.target.value))
                  )
                )
              }
            />
            {pollIntervalMs >= pollDisabledAtMs ? 'off' : `${pollIntervalMs}ms`}
          </label>

          <button
            type="button"
            onClick={onClearSelection}
            style={{
              border: '1px solid rgba(148, 163, 184, 0.4)',
              borderRadius: 6,
              background: 'rgba(15, 23, 42, 0.8)',
              color: '#e2e8f0',
              padding: '4px 8px',
              fontSize: 12,
              cursor: 'pointer',
            }}
          >
            Close
          </button>
        </div>
      </div>

      <div
        style={{
          borderBottom: activeEntity
            ? '1px solid rgba(148, 163, 184, 0.2)'
            : 'none',
          flex: activeEntity ? '0 0 auto' : '1 1 auto',
          maxHeight: activeEntity ? 180 : undefined,
          minHeight: 0,
          overflow: 'auto',
          padding: '8px 10px',
          display: 'flex',
          flexDirection: 'column',
          gap: 6,
        }}
      >
        {visibleEntities.map((entity) => {
          const isActive = entity.entityId === activeEntityId;
          const isHovered = entity.entityId === hoveredEntityId;

          return (
            <button
              key={entity.entityId}
              type="button"
              onMouseEnter={() => onHoverEntity(entity.entityId)}
              onMouseLeave={() => onHoverEntity(null)}
              onClick={() => onSelectEntity(entity.entityId)}
              style={{
                textAlign: 'left',
                border: `1px solid ${
                  isActive
                    ? 'rgba(59, 130, 246, 0.95)'
                    : isHovered
                      ? 'rgba(96, 165, 250, 0.7)'
                      : 'rgba(148, 163, 184, 0.28)'
                }`,
                borderRadius: 8,
                background: isHovered
                  ? 'rgba(30, 64, 175, 0.4)'
                  : isActive
                    ? 'rgba(30, 58, 138, 0.55)'
                    : 'rgba(15, 23, 42, 0.7)',
                color: '#e2e8f0',
                padding: '6px 8px',
                cursor: 'pointer',
              }}
            >
              <div
                style={{
                  fontFamily:
                    'ui-monospace, SFMono-Regular, Menlo, Consolas, monospace',
                  fontSize: 12,
                  whiteSpace: 'nowrap',
                  overflow: 'hidden',
                  textOverflow: 'ellipsis',
                }}
                title={entity.entityId}
              >
                {entity.entityId}
              </div>
              <div style={{ fontSize: 11, opacity: 0.82 }}>
                owner: {entity.ownerId} | client: {entity.clientId || '-'} | world:{' '}
                {entity.world}
              </div>
              <div style={{ fontSize: 11, opacity: 0.74 }}>
                pos: ({formatNumber(entity.x)}, {formatNumber(entity.y)},{' '}
                {formatNumber(entity.z)})
              </div>
            </button>
          );
        })}
      </div>

      <div
        style={{
          flex: activeEntity ? '1 1 auto' : '0 0 auto',
          minHeight: 0,
          maxHeight: activeEntity ? '1000px' : '0px',
          opacity: activeEntity ? 1 : 0,
          transform: activeEntity ? 'translateY(0px)' : 'translateY(18px)',
          transition:
            'max-height 220ms ease, opacity 180ms ease, transform 220ms ease',
          overflow: 'hidden',
        }}
      >
        <div
          style={{
            height: '100%',
            minHeight: 0,
            overflow: 'hidden',
            padding: activeEntity ? 14 : 0,
            display: 'flex',
            flexDirection: 'column',
            gap: 10,
            pointerEvents: activeEntity ? 'auto' : 'none',
          }}
        >
          <div style={{ fontSize: 12, opacity: 0.82 }}>
            Click the highlighted entity row again to collapse details.
          </div>

          {activeEntity && details.loading && !details.data && (
            <div style={{ fontSize: 12, opacity: 0.85 }}>
              Loading database details...
            </div>
          )}

          {activeEntity && details.loading && details.data && (
            <div style={{ fontSize: 12, opacity: 0.75 }}>Refreshing details...</div>
          )}

          {activeEntity && details.error && !details.data && (
            <div
              style={{
                fontSize: 12,
                color: '#fecaca',
                border: '1px solid rgba(248, 113, 113, 0.35)',
                borderRadius: 8,
                background: 'rgba(127, 29, 29, 0.25)',
                padding: '6px 8px',
              }}
            >
              {details.error}
            </div>
          )}

          {activeEntity && details.data && (
            <>
              <div
                style={{
                  fontSize: 12,
                  opacity: 0.85,
                  display: 'flex',
                  flexWrap: 'wrap',
                  gap: 10,
                }}
              >
                <span>matches: {details.data.totalMatches}</span>
                <span>sources: {details.data.sourceSummaries.length}</span>
                {details.data.truncated && <span>truncated</span>}
              </div>

              <div style={{ fontSize: 11, opacity: 0.74 }}>
                search terms: {details.data.terms.join(', ')}
              </div>

              <DatabaseExplorerInspector
                records={details.data.records}
                loading={false}
                selectionScopeKey={activeEntity.entityId}
                fillHeight
                containerClassName="pb-2 flex-1 min-h-0 flex flex-col"
                controlsClassName="!mb-2"
                explorerViewportClassName="h-full min-h-0 overflow-auto p-2"
                inspectorViewportClassName="h-full min-h-0 overflow-auto p-3"
                desktopGridColumnsClassName={
                  ENTITY_INSPECTOR_DESKTOP_GRID_COLUMNS_CLASS
                }
              />
            </>
          )}

          {activeEntity && details.error && details.data && (
            <div
              style={{
                fontSize: 12,
                color: '#fecaca',
                border: '1px solid rgba(248, 113, 113, 0.35)',
                borderRadius: 8,
                background: 'rgba(127, 29, 29, 0.25)',
                padding: '6px 8px',
              }}
            >
              {details.error}
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
