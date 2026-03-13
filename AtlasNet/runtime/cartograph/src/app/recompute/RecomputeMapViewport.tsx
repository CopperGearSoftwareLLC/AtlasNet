'use client';

import { useEffect, useMemo, useRef, useState } from 'react';
import type { ShapeJS } from '../shared/cartographTypes';
import {
  createMapRenderer,
  type MapProjectionMode,
  type MapViewMode,
} from '../map/core/mapRenderer';
import type { RecomputeOverlayLayer } from './recomputeOverlayShapes';

interface RecomputeMapViewportProps {
  baseShapes: ShapeJS[];
  overlayLayers: RecomputeOverlayLayer[];
  activeLayerIds: Set<string>;
  boundsStatusText: string;
}

export function RecomputeMapViewport({
  baseShapes,
  overlayLayers,
  activeLayerIds,
  boundsStatusText,
}: RecomputeMapViewportProps) {
  const containerRef = useRef<HTMLDivElement>(null);
  const rendererRef = useRef<ReturnType<typeof createMapRenderer> | null>(null);
  const [viewMode, setViewMode] = useState<MapViewMode>('2d');
  const [projectionMode, setProjectionMode] =
    useState<MapProjectionMode>('orthographic');

  const shapes = useMemo(() => {
    const overlays = overlayLayers
      .filter((layer) => activeLayerIds.has(layer.id))
      .flatMap((layer) => layer.shapes);
    return [...baseShapes, ...overlays];
  }, [activeLayerIds, baseShapes, overlayLayers]);

  useEffect(() => {
    const container = containerRef.current;
    if (!container || rendererRef.current) {
      return;
    }

    rendererRef.current = createMapRenderer({
      container,
      shapes: [],
      viewMode,
      projectionMode,
      interactionSensitivity: 1,
    });

    return () => {
      rendererRef.current?.destroy();
      rendererRef.current = null;
    };
  }, [projectionMode, viewMode]);

  useEffect(() => {
    rendererRef.current?.setShapes(shapes);
  }, [shapes]);

  useEffect(() => {
    rendererRef.current?.setViewMode(viewMode);
  }, [viewMode]);

  useEffect(() => {
    rendererRef.current?.setProjectionMode(projectionMode);
  }, [projectionMode]);

  return (
    <section className="rounded-3xl border border-slate-800 bg-slate-900/70 overflow-hidden">
      <div className="flex flex-wrap items-center justify-between gap-3 border-b border-slate-800 px-4 py-3">
        <div>
          <div className="text-xs uppercase tracking-[0.22em] text-cyan-400">
            Map Snapshot
          </div>
          <div className="mt-1 text-sm text-slate-300">
            Map bounds snapshot plus recompute overlays. Voronoi cycles keep
            their bounds in the `Cells` overlay instead.
          </div>
          <div className="mt-1 text-xs text-slate-500">{boundsStatusText}</div>
        </div>
        <div className="flex flex-wrap gap-2">
          <button
            type="button"
            onClick={() => setViewMode('2d')}
            className={
              'rounded-lg border px-3 py-1.5 text-xs ' +
              (viewMode === '2d'
                ? 'border-cyan-500 bg-cyan-500/10 text-cyan-200'
                : 'border-slate-700 text-slate-300 hover:bg-slate-800')
            }
          >
            2D
          </button>
          <button
            type="button"
            onClick={() => setViewMode('3d')}
            className={
              'rounded-lg border px-3 py-1.5 text-xs ' +
              (viewMode === '3d'
                ? 'border-cyan-500 bg-cyan-500/10 text-cyan-200'
                : 'border-slate-700 text-slate-300 hover:bg-slate-800')
            }
          >
            3D
          </button>
          <button
            type="button"
            onClick={() =>
              setProjectionMode((previous) =>
                previous === 'orthographic' ? 'perspective' : 'orthographic'
              )
            }
            className="rounded-lg border border-slate-700 px-3 py-1.5 text-xs text-slate-300 hover:bg-slate-800"
          >
            {projectionMode === 'orthographic' ? 'Ortho' : 'Perspective'}
          </button>
          <button
            type="button"
            onClick={() => rendererRef.current?.resetCamera()}
            className="rounded-lg border border-slate-700 px-3 py-1.5 text-xs text-slate-300 hover:bg-slate-800"
          >
            Reset
          </button>
        </div>
      </div>

      <div className="min-h-[420px] bg-slate-950">
        <div ref={containerRef} className="h-full min-h-[420px] w-full" />
        {overlayLayers.length === 0 ? (
          <div className="border-t border-slate-800 px-4 py-3 text-sm text-slate-500">
            No overlay geometry was derived from this capture yet.
          </div>
        ) : null}
        {overlayLayers.length > 0 && activeLayerIds.size === 0 ? (
          <div className="border-t border-slate-800 px-4 py-3 text-sm text-slate-500">
            All overlays are currently hidden.
          </div>
        ) : null}
        </div>
    </section>
  );
}
