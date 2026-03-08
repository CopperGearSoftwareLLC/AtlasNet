'use client';

import { useEffect, useRef, useState } from 'react';
import type {
  AuthorityEntityTelemetry,
  ShardPlacementTelemetry,
  ShapeJS,
  ShardTelemetry,
  TransferManifestTelemetry,
  TransferStateQueueTelemetry,
  WorkersSnapshotResponse,
} from './cartographTypes';
import {
  parseAuthorityRows,
  parseTransferManifestRows,
  parseTransferStateQueueRows,
} from './telemetryParsers';

interface PolledResourceOptions<T> {
  url: string;
  intervalMs: number;
  enabled?: boolean;
  createInitialValue: () => T;
  mapResponse: (raw: unknown) => T;
  resetOnException?: boolean;
  resetOnHttpError?: boolean;
  onException?: (err: unknown) => void;
  onHttpError?: (statusCode: number) => void;
  onPollResult?: (meta: {
    pollStartedAtMs: number;
    pollCompletedAtMs: number;
    succeeded: boolean;
    statusCode: number | null;
  }) => void;
}

function toShapeArray(raw: unknown): ShapeJS[] {
  return Array.isArray(raw) ? (raw as ShapeJS[]) : [];
}

function toShardTelemetryArray(raw: unknown): ShardTelemetry[] {
  return Array.isArray(raw) ? (raw as ShardTelemetry[]) : [];
}

function toWorkersSnapshot(raw: unknown): WorkersSnapshotResponse {
  if (!raw || typeof raw !== 'object') {
    return {
      collectedAtMs: null,
      dockerCliAvailable: false,
      error: null,
      contexts: [],
    };
  }

  const payload = raw as Partial<WorkersSnapshotResponse>;
  return {
    collectedAtMs:
      typeof payload.collectedAtMs === 'number' && Number.isFinite(payload.collectedAtMs)
        ? payload.collectedAtMs
        : null,
    dockerCliAvailable: Boolean(payload.dockerCliAvailable),
    error: typeof payload.error === 'string' && payload.error.trim().length > 0
      ? payload.error
      : null,
    contexts: Array.isArray(payload.contexts) ? payload.contexts : [],
  };
}

function toShardPlacementArray(raw: unknown): ShardPlacementTelemetry[] {
  return Array.isArray(raw) ? (raw as ShardPlacementTelemetry[]) : [];
}

function usePolledResource<T>({
  url,
  intervalMs,
  enabled = true,
  createInitialValue,
  mapResponse,
  resetOnException = false,
  resetOnHttpError = false,
  onException,
  onHttpError,
  onPollResult,
}: PolledResourceOptions<T>): T {
  const [data, setData] = useState<T>(() => createInitialValue());
  const createInitialValueRef = useRef(createInitialValue);
  const mapResponseRef = useRef(mapResponse);
  const onExceptionRef = useRef(onException);
  const onHttpErrorRef = useRef(onHttpError);
  const onPollResultRef = useRef(onPollResult);

  createInitialValueRef.current = createInitialValue;
  mapResponseRef.current = mapResponse;
  onExceptionRef.current = onException;
  onHttpErrorRef.current = onHttpError;
  onPollResultRef.current = onPollResult;

  useEffect(() => {
    if (!enabled) {
      setData(createInitialValueRef.current());
      return;
    }

    let alive = true;
    let inFlight = false;

    async function poll() {
      if (inFlight) {
        return;
      }
      inFlight = true;
      const pollStartedAtMs = Date.now();

      try {
        const response = await fetch(url, { cache: 'no-store' });
        if (!response.ok) {
          onHttpErrorRef.current?.(response.status);
          onPollResultRef.current?.({
            pollStartedAtMs,
            pollCompletedAtMs: Date.now(),
            succeeded: false,
            statusCode: response.status,
          });
          if (alive && resetOnHttpError) {
            setData(createInitialValueRef.current());
          }
          return;
        }

        const raw = await response.json();
        if (!alive) {
          return;
        }

        setData(mapResponseRef.current(raw));
        onPollResultRef.current?.({
          pollStartedAtMs,
          pollCompletedAtMs: Date.now(),
          succeeded: true,
          statusCode: response.status,
        });
      } catch (err) {
        onExceptionRef.current?.(err);
        onPollResultRef.current?.({
          pollStartedAtMs,
          pollCompletedAtMs: Date.now(),
          succeeded: false,
          statusCode: null,
        });
        if (alive && resetOnException) {
          setData(createInitialValueRef.current());
        }
      } finally {
        inFlight = false;
      }
    }

    void poll();
    if (intervalMs <= 0) {
      return () => {
        alive = false;
      };
    }

    const intervalId = setInterval(() => {
      void poll();
    }, intervalMs);

    return () => {
      alive = false;
      clearInterval(intervalId);
    };
  }, [
    enabled,
    intervalMs,
    resetOnException,
    resetOnHttpError,
    url,
  ]);

  return data;
}

interface TelemetryPollingOptions {
  intervalMs: number;
  enabled?: boolean;
  resetOnException?: boolean;
  resetOnHttpError?: boolean;
  onException?: (err: unknown) => void;
  onHttpError?: (statusCode: number) => void;
  onPollResult?: (meta: {
    pollStartedAtMs: number;
    pollCompletedAtMs: number;
    succeeded: boolean;
    statusCode: number | null;
  }) => void;
}

export function useHeuristicShapes({
  intervalMs = 0,
  enabled = true,
  resetOnException = false,
  resetOnHttpError = false,
  onException,
  onHttpError,
}: Partial<TelemetryPollingOptions> = {}): ShapeJS[] {
  return usePolledResource<ShapeJS[]>({
    url: '/api/heuristicfetch',
    intervalMs,
    enabled,
    createInitialValue: () => [],
    mapResponse: toShapeArray,
    resetOnException,
    resetOnHttpError,
    onException,
    onHttpError,
  });
}

export function useNetworkTelemetry({
  intervalMs,
  enabled = true,
  resetOnException = false,
  resetOnHttpError = false,
  onException,
  onHttpError,
}: TelemetryPollingOptions): ShardTelemetry[] {
  return usePolledResource<ShardTelemetry[]>({
    url: '/api/networktelemetry',
    intervalMs,
    enabled,
    createInitialValue: () => [],
    mapResponse: toShardTelemetryArray,
    resetOnException,
    resetOnHttpError,
    onException,
    onHttpError,
  });
}

export function useAuthorityEntities({
  intervalMs,
  enabled = true,
  resetOnException = false,
  resetOnHttpError = false,
  onException,
  onHttpError,
}: TelemetryPollingOptions): AuthorityEntityTelemetry[] {
  return usePolledResource<AuthorityEntityTelemetry[]>({
    url: '/api/authoritytelemetry',
    intervalMs,
    enabled,
    createInitialValue: () => [],
    mapResponse: parseAuthorityRows,
    resetOnException,
    resetOnHttpError,
    onException,
    onHttpError,
  });
}

export function useTransferManifest({
  intervalMs,
  enabled = true,
  resetOnException = false,
  resetOnHttpError = false,
  onException,
  onHttpError,
}: TelemetryPollingOptions): TransferManifestTelemetry[] {
  return usePolledResource<TransferManifestTelemetry[]>({
    url: '/api/transfermanifest',
    intervalMs,
    enabled,
    createInitialValue: () => [],
    mapResponse: parseTransferManifestRows,
    resetOnException,
    resetOnHttpError,
    onException,
    onHttpError,
  });
}

export function useTransferStateQueue({
  intervalMs,
  enabled = true,
  resetOnException = false,
  resetOnHttpError = false,
  onException,
  onHttpError,
  onPollResult,
}: TelemetryPollingOptions): TransferStateQueueTelemetry[] {
  return usePolledResource<TransferStateQueueTelemetry[]>({
    url: '/api/transferstatequeue',
    intervalMs,
    enabled,
    createInitialValue: () => [],
    mapResponse: parseTransferStateQueueRows,
    resetOnException,
    resetOnHttpError,
    onException,
    onHttpError,
    onPollResult,
  });
}

export function useWorkersSnapshot({
  intervalMs,
  enabled = true,
  resetOnException = false,
  resetOnHttpError = false,
  onException,
  onHttpError,
}: TelemetryPollingOptions): WorkersSnapshotResponse {
  return usePolledResource<WorkersSnapshotResponse>({
    url: '/api/workers',
    intervalMs,
    enabled,
    createInitialValue: () => ({
      collectedAtMs: null,
      dockerCliAvailable: false,
      error: null,
      contexts: [],
    }),
    mapResponse: toWorkersSnapshot,
    resetOnException,
    resetOnHttpError,
    onException,
    onHttpError,
  });
}

export function useShardPlacement({
  intervalMs,
  enabled = true,
  resetOnException = false,
  resetOnHttpError = false,
  onException,
  onHttpError,
}: TelemetryPollingOptions): ShardPlacementTelemetry[] {
  return usePolledResource<ShardPlacementTelemetry[]>({
    url: '/api/shard-placement',
    intervalMs,
    enabled,
    createInitialValue: () => [],
    mapResponse: toShardPlacementArray,
    resetOnException,
    resetOnHttpError,
    onException,
    onHttpError,
  });
}
