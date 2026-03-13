'use client';

import { useCallback, useEffect, useRef } from 'react';
import type { Dispatch, SetStateAction } from 'react';
import {
  pointWithinShardRegion,
  type Point2,
  type ShardHoverBounds,
  type ShardHoverRegion,
} from '../core/mapData';

interface UseShardHoverStateArgs {
  showShardHoverDetails: boolean;
  networkNodeIdSet: Set<string>;
  shardHoverBoundsById: Map<string, ShardHoverBounds>;
  shardHoverRegionsById: Map<string, ShardHoverRegion[]>;
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

function pointWithinAnyPolygon(point: Point2, polygons: Point2[][]): boolean {
  for (const polygon of polygons) {
    if (
      pointWithinShardRegion(point, {
        kind: 'polygon',
        points: polygon,
        area: Number.POSITIVE_INFINITY,
      })
    ) {
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
  shardHoverRegionsById,
  shardHoverPolygonsById,
  showShardHoverDetails,
}: UseShardHoverStateArgs): UseShardHoverStateResult {
  const showShardHoverDetailsRef = useRef(showShardHoverDetails);
  const shardHoverBoundsByIdRef = useRef<Map<string, ShardHoverBounds>>(new Map());
  const shardHoverRegionsByIdRef = useRef<Map<string, ShardHoverRegion[]>>(new Map());
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
    shardHoverRegionsByIdRef.current = shardHoverRegionsById;
  }, [shardHoverRegionsById]);

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
      let nearestInfiniteDistance = Infinity;

      for (const shardId of networkNodeIdSetRef.current) {
        const bounds = shardHoverBoundsByIdRef.current.get(shardId);
        const regions = shardHoverRegionsByIdRef.current.get(shardId) ?? [];

        if (regions.length > 0) {
          let matchedArea = Infinity;
          let matchedInfiniteDistance = Infinity;
          let matchedRegion = false;

          for (const region of regions) {
            if (!pointWithinShardRegion(point, region)) {
              continue;
            }
            matchedRegion = true;
            if (Number.isFinite(region.area)) {
              matchedArea = Math.min(matchedArea, region.area);
              continue;
            }
            if (region.kind === 'halfPlaneCell') {
              const dx = point.x - region.site.x;
              const dy = point.y - region.site.y;
              matchedInfiniteDistance = Math.min(
                matchedInfiniteDistance,
                Math.hypot(dx, dy)
              );
            }
          }

          if (!matchedRegion) {
            continue;
          }

          if (
            matchedArea < smallestArea ||
            (!Number.isFinite(matchedArea) &&
              !Number.isFinite(smallestArea) &&
              matchedInfiniteDistance < nearestInfiniteDistance)
          ) {
            smallestArea = matchedArea;
            nearestInfiniteDistance = matchedInfiniteDistance;
            nextHoveredId = shardId;
          }
          continue;
        }

        if (!bounds) {
          continue;
        }

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
          nearestInfiniteDistance = Infinity;
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
