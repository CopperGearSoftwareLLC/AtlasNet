'use client';

import { startTransition, useCallback, useEffect, useRef, useState } from 'react';
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
} from './parseTelemetryPayloads';

interface PolledResourceOptions<T> {
  url: string;
  intervalMs: number;
  enabled?: boolean;
  abortInFlightOnNextPoll?: boolean;
  createInitialValue: () => T;
  mapResponse: (raw: unknown) => T;
  areEqual?: (left: T, right: T) => boolean;
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

const EMPTY_SHAPES: ShapeJS[] = [];
const EMPTY_SHARD_TELEMETRY: ShardTelemetry[] = [];
const EMPTY_SHARD_PLACEMENT: ShardPlacementTelemetry[] = [];
const EMPTY_WORKERS_CONTEXTS: WorkersSnapshotResponse['contexts'] = [];
const EMPTY_WORKERS_SNAPSHOT: WorkersSnapshotResponse = {
  collectedAtMs: null,
  dockerCliAvailable: false,
  error: null,
  contexts: EMPTY_WORKERS_CONTEXTS,
};

function toShapeArray(raw: unknown): ShapeJS[] {
  if (!Array.isArray(raw) || raw.length === 0) {
    return EMPTY_SHAPES;
  }
  return raw as ShapeJS[];
}

function toShardTelemetryArray(raw: unknown): ShardTelemetry[] {
  if (!Array.isArray(raw) || raw.length === 0) {
    return EMPTY_SHARD_TELEMETRY;
  }
  return raw as ShardTelemetry[];
}

function toWorkersSnapshot(raw: unknown): WorkersSnapshotResponse {
  if (!raw || typeof raw !== 'object') {
    return EMPTY_WORKERS_SNAPSHOT;
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
    contexts: Array.isArray(payload.contexts) ? payload.contexts : EMPTY_WORKERS_CONTEXTS,
  };
}

function toShardPlacementArray(raw: unknown): ShardPlacementTelemetry[] {
  if (!Array.isArray(raw) || raw.length === 0) {
    return EMPTY_SHARD_PLACEMENT;
  }
  return raw as ShardPlacementTelemetry[];
}

function usePolledResource<T>({
  url,
  intervalMs,
  enabled = true,
  abortInFlightOnNextPoll = false,
  createInitialValue,
  mapResponse,
  areEqual,
  resetOnException = false,
  resetOnHttpError = false,
  onException,
  onHttpError,
  onPollResult,
}: PolledResourceOptions<T>): T {
  const [data, setData] = useState<T>(() => createInitialValue());
  const createInitialValueRef = useRef(createInitialValue);
  const mapResponseRef = useRef(mapResponse);
  const areEqualRef = useRef(areEqual);
  const onExceptionRef = useRef(onException);
  const onHttpErrorRef = useRef(onHttpError);
  const onPollResultRef = useRef(onPollResult);

  createInitialValueRef.current = createInitialValue;
  mapResponseRef.current = mapResponse;
  areEqualRef.current = areEqual;
  onExceptionRef.current = onException;
  onHttpErrorRef.current = onHttpError;
  onPollResultRef.current = onPollResult;

  const updateData = useCallback((next: T): void => {
    startTransition(() => {
      setData((previous) => {
        const equals = areEqualRef.current
          ? areEqualRef.current(previous, next)
          : Object.is(previous, next);
        return equals ? previous : next;
      });
    });
  }, []);

  useEffect(() => {
    if (!enabled) {
      updateData(createInitialValueRef.current());
      return;
    }

    let alive = true;
    let activeController: AbortController | null = null;
    let requestSequence = 0;

    async function poll() {
      if (activeController) {
        if (!abortInFlightOnNextPoll) {
          return;
        }
        activeController.abort();
      }
      requestSequence += 1;
      const requestId = requestSequence;
      const controller = new AbortController();
      activeController = controller;
      const pollStartedAtMs = Date.now();

      try {
        const response = await fetch(url, {
          cache: 'no-store',
          signal: controller.signal,
        });
        if (!response.ok) {
          onHttpErrorRef.current?.(response.status);
          onPollResultRef.current?.({
            pollStartedAtMs,
            pollCompletedAtMs: Date.now(),
            succeeded: false,
            statusCode: response.status,
          });
          if (alive && resetOnHttpError) {
            updateData(createInitialValueRef.current());
          }
          return;
        }

        const raw = await response.json();
        if (!alive || requestId !== requestSequence) {
          return;
        }

        updateData(mapResponseRef.current(raw));
        onPollResultRef.current?.({
          pollStartedAtMs,
          pollCompletedAtMs: Date.now(),
          succeeded: true,
          statusCode: response.status,
        });
      } catch (err) {
        if (controller.signal.aborted) {
          return;
        }
        if (requestId !== requestSequence) {
          return;
        }
        onExceptionRef.current?.(err);
        onPollResultRef.current?.({
          pollStartedAtMs,
          pollCompletedAtMs: Date.now(),
          succeeded: false,
          statusCode: null,
        });
        if (alive && resetOnException) {
          updateData(createInitialValueRef.current());
        }
      } finally {
        if (activeController === controller) {
          activeController = null;
        }
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
      activeController?.abort();
      clearInterval(intervalId);
    };
  }, [
    enabled,
    intervalMs,
    abortInFlightOnNextPoll,
    resetOnException,
    resetOnHttpError,
    updateData,
    url,
  ]);

  return data;
}

interface TelemetryPollingOptions {
  intervalMs: number;
  enabled?: boolean;
  abortInFlightOnNextPoll?: boolean;
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
    url: '/api/heuristic-shapes',
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
  includeLiveIds = true,
}: TelemetryPollingOptions & { includeLiveIds?: boolean }): ShardTelemetry[] {
  const query = includeLiveIds ? '' : '?liveIds=0';
  return usePolledResource<ShardTelemetry[]>({
    url: `/api/networktelemetry${query}`,
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
  abortInFlightOnNextPoll = false,
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
    abortInFlightOnNextPoll,
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
