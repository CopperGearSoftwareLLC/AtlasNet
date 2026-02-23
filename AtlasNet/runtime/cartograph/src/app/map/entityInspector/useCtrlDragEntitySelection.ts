'use client';

import { useEffect, useMemo, useRef, useState } from 'react';
import type { PointerEventHandler, RefObject } from 'react';
import type { AuthorityEntityTelemetry } from '../../lib/cartographTypes';

interface MapPoint {
  x: number;
  y: number;
}

interface SelectionRect {
  left: number;
  top: number;
  width: number;
  height: number;
}

interface MapProjector {
  projectMapPoint(point: {
    x: number;
    y: number;
    z?: number;
  }): MapPoint | null;
}

interface UseCtrlDragEntitySelectionArgs {
  containerRef: RefObject<HTMLDivElement | null>;
  rendererRef: RefObject<MapProjector | null>;
  entities: AuthorityEntityTelemetry[];
}

interface UseCtrlDragEntitySelectionResult {
  selectedEntities: AuthorityEntityTelemetry[];
  selectedEntityIds: string[];
  activeEntityId: string | null;
  hoveredEntityId: string | null;
  selectionRect: SelectionRect | null;
  clearSelection: () => void;
  setActiveEntityId: (entityId: string | null) => void;
  setHoveredEntityId: (entityId: string | null) => void;
  onPointerDownCapture: PointerEventHandler<HTMLDivElement>;
  onPointerMoveCapture: PointerEventHandler<HTMLDivElement>;
  onPointerUpCapture: PointerEventHandler<HTMLDivElement>;
  onPointerCancelCapture: PointerEventHandler<HTMLDivElement>;
}

interface DragState {
  pointerId: number;
  start: MapPoint;
  current: MapPoint;
}

function normalizeSelectionRect(start: MapPoint, end: MapPoint): SelectionRect {
  const left = Math.min(start.x, end.x);
  const top = Math.min(start.y, end.y);
  return {
    left,
    top,
    width: Math.abs(end.x - start.x),
    height: Math.abs(end.y - start.y),
  };
}

function pointInRect(point: MapPoint, rect: SelectionRect): boolean {
  return (
    point.x >= rect.left &&
    point.x <= rect.left + rect.width &&
    point.y >= rect.top &&
    point.y <= rect.top + rect.height
  );
}

function getRelativePoint(
  container: HTMLDivElement,
  clientX: number,
  clientY: number
): MapPoint {
  const rect = container.getBoundingClientRect();
  return {
    x: clientX - rect.left,
    y: clientY - rect.top,
  };
}

const MIN_SELECTION_PIXELS = 6;

export function useCtrlDragEntitySelection({
  containerRef,
  entities,
  rendererRef,
}: UseCtrlDragEntitySelectionArgs): UseCtrlDragEntitySelectionResult {
  const [dragState, setDragState] = useState<DragState | null>(null);
  const [selectedEntityIds, setSelectedEntityIds] = useState<string[]>([]);
  const [activeEntityId, setActiveEntityId] = useState<string | null>(null);
  const [hoveredEntityId, setHoveredEntityId] = useState<string | null>(null);
  const dragStateRef = useRef<DragState | null>(null);
  const lastKnownEntitiesByIdRef = useRef(
    new Map<string, AuthorityEntityTelemetry>()
  );

  const entitiesById = useMemo(() => {
    const out = new Map<string, AuthorityEntityTelemetry>();
    for (const entity of entities) {
      if (entity.entityId && !out.has(entity.entityId)) {
        out.set(entity.entityId, entity);
      }
    }
    return out;
  }, [entities]);

  useEffect(() => {
    dragStateRef.current = dragState;
  }, [dragState]);

  useEffect(() => {
    const lastKnown = lastKnownEntitiesByIdRef.current;
    for (const entity of entities) {
      if (entity.entityId) {
        lastKnown.set(entity.entityId, entity);
      }
    }
  }, [entities]);

  useEffect(() => {
    const selectedIdSet = new Set(selectedEntityIds);
    setActiveEntityId((previous) =>
      previous && selectedIdSet.has(previous) ? previous : null
    );
    setHoveredEntityId((previous) =>
      previous && selectedIdSet.has(previous) ? previous : null
    );
  }, [selectedEntityIds]);

  const selectionRect = useMemo(() => {
    if (!dragState) {
      return null;
    }
    return normalizeSelectionRect(dragState.start, dragState.current);
  }, [dragState]);

  const selectedEntities = useMemo(() => {
    const selected: AuthorityEntityTelemetry[] = [];
    const lastKnown = lastKnownEntitiesByIdRef.current;

    for (const entityId of selectedEntityIds) {
      const live = entitiesById.get(entityId);
      if (live) {
        lastKnown.set(entityId, live);
        selected.push(live);
        continue;
      }

      const fallback = lastKnown.get(entityId);
      if (fallback) {
        selected.push(fallback);
      }
    }

    return selected;
  }, [entitiesById, selectedEntityIds]);

  function clearSelection(): void {
    setSelectedEntityIds([]);
    setActiveEntityId(null);
    setHoveredEntityId(null);
  }

  function completeDragSelection(container: HTMLDivElement, pointerId: number): void {
    const activeDrag = dragStateRef.current;
    if (!activeDrag || activeDrag.pointerId !== pointerId) {
      return;
    }

    let rect = normalizeSelectionRect(activeDrag.start, activeDrag.current);
    if (rect.width < MIN_SELECTION_PIXELS && rect.height < MIN_SELECTION_PIXELS) {
      rect = {
        left: activeDrag.start.x - MIN_SELECTION_PIXELS,
        top: activeDrag.start.y - MIN_SELECTION_PIXELS,
        width: MIN_SELECTION_PIXELS * 2,
        height: MIN_SELECTION_PIXELS * 2,
      };
    }

    const projector = rendererRef.current;
    if (!projector) {
      setDragState(null);
      dragStateRef.current = null;
      try {
        if (container.hasPointerCapture(pointerId)) {
          container.releasePointerCapture(pointerId);
        }
      } catch {}
      return;
    }

    const selectedIds = Array.from(entitiesById.values())
      .filter((entity) => {
        const projected = projector.projectMapPoint({
          x: entity.x,
          y: entity.y,
          z: entity.z,
        });
        return projected ? pointInRect(projected, rect) : false;
      })
      .map((entity) => entity.entityId)
      .sort((left, right) => left.localeCompare(right));

    setSelectedEntityIds(selectedIds);
    setHoveredEntityId(null);
    setActiveEntityId(null);
    setDragState(null);
    dragStateRef.current = null;

    try {
      if (container.hasPointerCapture(pointerId)) {
        container.releasePointerCapture(pointerId);
      }
    } catch {}
  }

  const onPointerDownCapture: PointerEventHandler<HTMLDivElement> = (event) => {
    if (!event.ctrlKey || event.button !== 0) {
      return;
    }

    const container = containerRef.current;
    if (!container) {
      return;
    }

    const point = getRelativePoint(container, event.clientX, event.clientY);
    const nextDrag: DragState = {
      pointerId: event.pointerId,
      start: point,
      current: point,
    };
    setDragState(nextDrag);
    dragStateRef.current = nextDrag;

    try {
      container.setPointerCapture(event.pointerId);
    } catch {}

    event.preventDefault();
    event.stopPropagation();
  };

  const onPointerMoveCapture: PointerEventHandler<HTMLDivElement> = (event) => {
    const activeDrag = dragStateRef.current;
    if (!activeDrag || activeDrag.pointerId !== event.pointerId) {
      return;
    }

    const container = containerRef.current;
    if (!container) {
      return;
    }

    const point = getRelativePoint(container, event.clientX, event.clientY);
    const nextDrag: DragState = {
      pointerId: activeDrag.pointerId,
      start: activeDrag.start,
      current: point,
    };
    setDragState(nextDrag);
    dragStateRef.current = nextDrag;

    event.preventDefault();
    event.stopPropagation();
  };

  const onPointerUpCapture: PointerEventHandler<HTMLDivElement> = (event) => {
    const activeDrag = dragStateRef.current;
    if (!activeDrag || activeDrag.pointerId !== event.pointerId) {
      return;
    }

    const container = containerRef.current;
    if (!container) {
      return;
    }

    completeDragSelection(container, event.pointerId);
    event.preventDefault();
    event.stopPropagation();
  };

  const onPointerCancelCapture: PointerEventHandler<HTMLDivElement> = (event) => {
    const activeDrag = dragStateRef.current;
    if (!activeDrag || activeDrag.pointerId !== event.pointerId) {
      return;
    }

    const container = containerRef.current;
    setDragState(null);
    dragStateRef.current = null;
    if (container) {
      try {
        if (container.hasPointerCapture(event.pointerId)) {
          container.releasePointerCapture(event.pointerId);
        }
      } catch {}
    }

    event.preventDefault();
    event.stopPropagation();
  };

  return {
    selectedEntities,
    selectedEntityIds,
    activeEntityId,
    hoveredEntityId,
    selectionRect,
    clearSelection,
    setActiveEntityId,
    setHoveredEntityId,
    onPointerDownCapture,
    onPointerMoveCapture,
    onPointerUpCapture,
    onPointerCancelCapture,
  };
}
