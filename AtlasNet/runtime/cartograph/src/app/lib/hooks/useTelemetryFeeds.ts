'use client';

import { useEffect, useRef, useState } from 'react';
import type {
  AuthorityEntityTelemetry,
  ShapeJS,
  ShardTelemetry,
} from '../cartographTypes';
import { parseAuthorityRows } from '../authorityTelemetryTypes';

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
}

function toShapeArray(raw: unknown): ShapeJS[] {
  return Array.isArray(raw) ? (raw as ShapeJS[]) : [];
}

function toShardTelemetryArray(raw: unknown): ShardTelemetry[] {
  return Array.isArray(raw) ? (raw as ShardTelemetry[]) : [];
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
}: PolledResourceOptions<T>): T {
  const [data, setData] = useState<T>(() => createInitialValue());
  const createInitialValueRef = useRef(createInitialValue);
  const mapResponseRef = useRef(mapResponse);
  const onExceptionRef = useRef(onException);
  const onHttpErrorRef = useRef(onHttpError);

  createInitialValueRef.current = createInitialValue;
  mapResponseRef.current = mapResponse;
  onExceptionRef.current = onException;
  onHttpErrorRef.current = onHttpError;

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

      try {
        const response = await fetch(url, { cache: 'no-store' });
        if (!response.ok) {
          onHttpErrorRef.current?.(response.status);
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
      } catch (err) {
        onExceptionRef.current?.(err);
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
