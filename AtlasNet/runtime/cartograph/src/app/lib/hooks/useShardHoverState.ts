'use client';

import { useCallback, useEffect, useRef } from 'react';
import type { Dispatch, SetStateAction } from 'react';
import type { ShardHoverBounds } from '../mapData';

interface Point2 {
  x: number;
  y: number;
}

interface UseShardHoverStateArgs {
  showShardHoverDetails: boolean;
  showGnsConnections: boolean;
  networkNodeIdSet: Set<string>;
  shardHoverBoundsById: Map<string, ShardHoverBounds>;
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

export function useShardHoverState({
  hoveredShardId,
  networkNodeIdSet,
  setHoveredShardAnchor,
  setHoveredShardId,
  shardHoverBoundsById,
  showGnsConnections,
  showShardHoverDetails,
}: UseShardHoverStateArgs): UseShardHoverStateResult {
  const showShardHoverDetailsRef = useRef(showShardHoverDetails);
  const showGnsConnectionsRef = useRef(showGnsConnections);
  const shardHoverBoundsByIdRef = useRef<Map<string, ShardHoverBounds>>(new Map());
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
    showGnsConnectionsRef.current = showGnsConnections;
  }, [showGnsConnections]);

  useEffect(() => {
    networkNodeIdSetRef.current = networkNodeIdSet;
  }, [networkNodeIdSet]);

  useEffect(() => {
    shardHoverBoundsByIdRef.current = shardHoverBoundsById;
  }, [shardHoverBoundsById]);

  useEffect(() => {
    if (!showGnsConnections) {
      clearHoveredShard();
      return;
    }
    if (hoveredShardId && !networkNodeIdSet.has(hoveredShardId)) {
      clearHoveredShard();
    }
  }, [clearHoveredShard, hoveredShardId, networkNodeIdSet, showGnsConnections]);

  const handleMapPointerWorld = useCallback(
    (point: Point2 | null, screen: Point2 | null) => {
      if (
        !showShardHoverDetailsRef.current ||
        !showGnsConnectionsRef.current ||
        !point ||
        !screen
      ) {
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
