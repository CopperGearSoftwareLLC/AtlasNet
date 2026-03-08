'use client';

import { useCallback, useEffect, useRef } from 'react';
import type { Dispatch, SetStateAction } from 'react';
import type { ShardHoverBounds } from './mapData';

interface Point2 {
  x: number;
  y: number;
}

interface UseShardHoverStateArgs {
  showShardHoverDetails: boolean;
  networkNodeIdSet: Set<string>;
  shardHoverBoundsById: Map<string, ShardHoverBounds>;
  shardHoverPolygonsById: Map<string, Point2[][]>;
  hoveredShardId: string | null;
  setHoveredShardId: Dispatch<SetStateAction<string | null>>;
  setHoveredShardAnchor: Dispatch<SetStateAction<Point2 | null>>;
}

interface UseShardHoverStateResult {
  clearHoveredShard: () => void;
  handleMapPointerWorld: (point: Point2 | null, screen: Point2 | null) => void;
}

function pointWithinBounds(point: Point2, bounds: ShardHoverBounds): boolean {
  return (
    point.x >= bounds.minX &&
    point.x <= bounds.maxX &&
    point.y >= bounds.minY &&
    point.y <= bounds.maxY
  );
}

function pointInPolygon(point: Point2, polygon: Point2[]): boolean {
  if (polygon.length < 3) {
    return false;
  }

  // Ray casting: count edge crossings to the right.
  let inside = false;
  for (let i = 0, j = polygon.length - 1; i < polygon.length; j = i, i += 1) {
    const xi = polygon[i].x;
    const yi = polygon[i].y;
    const xj = polygon[j].x;
    const yj = polygon[j].y;

    const intersects =
      yi > point.y !== yj > point.y &&
      point.x < ((xj - xi) * (point.y - yi)) / (yj - yi + 1e-12) + xi;
    if (intersects) {
      inside = !inside;
    }
  }
  return inside;
}

function pointWithinAnyPolygon(point: Point2, polygons: Point2[][]): boolean {
  for (const polygon of polygons) {
    if (pointInPolygon(point, polygon)) {
      return true;
    }
  }
  return false;
}

export function useShardHoverState({
  hoveredShardId,
  networkNodeIdSet,
  setHoveredShardAnchor,
  setHoveredShardId,
  shardHoverBoundsById,
  shardHoverPolygonsById,
  showShardHoverDetails,
}: UseShardHoverStateArgs): UseShardHoverStateResult {
  const showShardHoverDetailsRef = useRef(showShardHoverDetails);
  const shardHoverBoundsByIdRef = useRef<Map<string, ShardHoverBounds>>(new Map());
  const shardHoverPolygonsByIdRef = useRef<Map<string, Point2[][]>>(new Map());
  const networkNodeIdSetRef = useRef<Set<string>>(new Set());

  const clearHoveredShard = useCallback(() => {
    setHoveredShardId(null);
    setHoveredShardAnchor(null);
  }, [setHoveredShardAnchor, setHoveredShardId]);

  useEffect(() => {
    showShardHoverDetailsRef.current = showShardHoverDetails;
    if (!showShardHoverDetails) {
      clearHoveredShard();
    }
  }, [clearHoveredShard, showShardHoverDetails]);

  useEffect(() => {
    networkNodeIdSetRef.current = networkNodeIdSet;
  }, [networkNodeIdSet]);

  useEffect(() => {
    shardHoverBoundsByIdRef.current = shardHoverBoundsById;
  }, [shardHoverBoundsById]);

  useEffect(() => {
    shardHoverPolygonsByIdRef.current = shardHoverPolygonsById;
  }, [shardHoverPolygonsById]);

  useEffect(() => {
    if (hoveredShardId && !networkNodeIdSet.has(hoveredShardId)) {
      clearHoveredShard();
    }
  }, [clearHoveredShard, hoveredShardId, networkNodeIdSet]);

  const handleMapPointerWorld = useCallback(
    (point: Point2 | null, screen: Point2 | null) => {
      if (!showShardHoverDetailsRef.current || !point || !screen) {
        clearHoveredShard();
        return;
      }

      let nextHoveredId: string | null = null;
      let smallestArea = Infinity;

      for (const [shardId, bounds] of shardHoverBoundsByIdRef.current) {
        if (!networkNodeIdSetRef.current.has(shardId)) {
          continue;
        }
        if (!pointWithinBounds(point, bounds)) {
          continue;
        }

        const polygons = shardHoverPolygonsByIdRef.current.get(shardId);
        if (polygons && polygons.length > 0) {
          if (!pointWithinAnyPolygon(point, polygons)) {
            continue;
          }
        }

        if (bounds.area < smallestArea) {
          smallestArea = bounds.area;
          nextHoveredId = shardId;
        }
      }

      if (!nextHoveredId) {
        clearHoveredShard();
        return;
      }

      setHoveredShardId((prev) => (prev === nextHoveredId ? prev : nextHoveredId));
      setHoveredShardAnchor((prev) => {
        if (!prev) {
          return screen;
        }
        if (Math.abs(prev.x - screen.x) > 0.5 || Math.abs(prev.y - screen.y) > 0.5) {
          return screen;
        }
        return prev;
      });
    },
    [clearHoveredShard, setHoveredShardAnchor, setHoveredShardId]
  );

  return { clearHoveredShard, handleMapPointerWorld };
}
