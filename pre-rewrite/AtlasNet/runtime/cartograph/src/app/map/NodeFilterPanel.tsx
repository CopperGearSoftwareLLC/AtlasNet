'use client';

import type { CSSProperties } from 'react';
import type { ClusterNodeSummary } from './core/useMapNodeFilterData';

interface NodeFilterPanelProps {
  open: boolean;
  nodes: ClusterNodeSummary[];
  selectedNodeCount: number;
  totalShardCount: number;
  visibleShardCount: number;
  onToggleOpen: () => void;
  onShowAllNodes: () => void;
  onHideAllNodes: () => void;
  onToggleNode: (nodeName: string) => void;
  isNodeSelected: (nodeName: string) => boolean;
}

const PANEL_BG = 'rgba(15, 23, 42, 0.92)';
const PANEL_BORDER = '1px solid rgba(148, 163, 184, 0.34)';
const BUTTON_BASE_STYLE: CSSProperties = {
  borderRadius: 8,
  border: '1px solid rgba(148, 163, 184, 0.42)',
  color: '#e2e8f0',
  background: 'rgba(30, 41, 59, 0.82)',
  cursor: 'pointer',
};

export function NodeFilterPanel({
  isNodeSelected,
  nodes,
  onHideAllNodes,
  onShowAllNodes,
  onToggleNode,
  onToggleOpen,
  open,
  selectedNodeCount,
  totalShardCount,
  visibleShardCount,
}: NodeFilterPanelProps) {
  const hasNodes = nodes.length > 0;

  return (
    <div
      style={{
        position: 'absolute',
        top: 12,
        right: 12,
        zIndex: 4,
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'flex-end',
        gap: 8,
      }}
    >
      <button
        type="button"
        onClick={onToggleOpen}
        style={{
          ...BUTTON_BASE_STYLE,
          padding: '8px 12px',
          fontSize: 12,
          fontWeight: 600,
          backdropFilter: 'blur(8px)',
          background: open ? 'rgba(8, 47, 73, 0.95)' : PANEL_BG,
        }}
      >
        node filter {open ? 'hide' : 'show'} ({selectedNodeCount}/{nodes.length})
      </button>

      {open && (
        <div
          style={{
            width: 320,
            maxWidth: 'min(320px, calc(100vw - 24px))',
            maxHeight: 'min(420px, calc(100vh - 120px))',
            overflow: 'hidden',
            borderRadius: 12,
            border: PANEL_BORDER,
            background: PANEL_BG,
            color: '#e2e8f0',
            boxShadow: '0 20px 45px rgba(2, 6, 23, 0.45)',
            backdropFilter: 'blur(10px)',
          }}
        >
          <div
            style={{
              padding: '12px 14px 10px',
              borderBottom: PANEL_BORDER,
              display: 'flex',
              flexDirection: 'column',
              gap: 8,
            }}
          >
            <div>
              <div style={{ fontSize: 13, fontWeight: 700 }}>Cluster nodes</div>
              <div style={{ fontSize: 11, opacity: 0.76, marginTop: 2 }}>
                Selected-node shards stay at full opacity on the map.
              </div>
            </div>

            <div style={{ display: 'flex', gap: 8, flexWrap: 'wrap' }}>
              <button
                type="button"
                onClick={onShowAllNodes}
                style={{ ...BUTTON_BASE_STYLE, padding: '5px 9px', fontSize: 12 }}
                disabled={!hasNodes}
              >
                all nodes
              </button>
              <button
                type="button"
                onClick={onHideAllNodes}
                style={{ ...BUTTON_BASE_STYLE, padding: '5px 9px', fontSize: 12 }}
                disabled={!hasNodes}
              >
                hide all
              </button>
              <div style={{ fontSize: 12, opacity: 0.82, alignSelf: 'center' }}>
                shards: {visibleShardCount}/{totalShardCount}
              </div>
            </div>
          </div>

          <div
            style={{
              maxHeight: 280,
              overflowY: 'auto',
              padding: '8px 10px 10px',
            }}
          >
            {!hasNodes && (
              <div
                style={{
                  fontSize: 12,
                  opacity: 0.74,
                  padding: '10px 4px',
                }}
              >
                No shard placement nodes are available yet.
              </div>
            )}

            {nodes.map((node) => {
              const checked = isNodeSelected(node.nodeName);
              return (
                <label
                  key={node.nodeName}
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    gap: 12,
                    padding: '8px 10px',
                    borderRadius: 9,
                    marginBottom: 6,
                    background: checked
                      ? 'rgba(15, 118, 110, 0.2)'
                      : 'rgba(30, 41, 59, 0.42)',
                    border: checked
                      ? '1px solid rgba(45, 212, 191, 0.38)'
                      : '1px solid rgba(148, 163, 184, 0.16)',
                    cursor: 'pointer',
                  }}
                >
                  <span
                    style={{
                      display: 'inline-flex',
                      alignItems: 'center',
                      gap: 10,
                      minWidth: 0,
                    }}
                  >
                    <input
                      type="checkbox"
                      checked={checked}
                      onChange={() => onToggleNode(node.nodeName)}
                    />
                    <span
                      style={{
                        fontSize: 12,
                        fontFamily:
                          'ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace',
                        whiteSpace: 'nowrap',
                        overflow: 'hidden',
                        textOverflow: 'ellipsis',
                      }}
                      title={node.nodeName}
                    >
                      {node.nodeName}
                    </span>
                  </span>
                  <span style={{ fontSize: 11, opacity: 0.72 }}>
                    {node.shardCount} shard{node.shardCount === 1 ? '' : 's'}
                  </span>
                </label>
              );
            })}
          </div>
        </div>
      )}
    </div>
  );
}
