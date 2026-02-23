'use client';

import { useEffect, useState } from 'react';
import type { AuthorityEntityTelemetry } from '../../lib/cartographTypes';
import type { EntityInspectorLookupResponse } from './types';

interface EntityDatabaseDetailsState {
  loading: boolean;
  error: string | null;
  data: EntityInspectorLookupResponse | null;
}

interface LookupContext {
  entityId: string;
  ownerId: string;
  clientId: string;
  world: number;
}

const POLL_DISABLED_AT_MS = 5000;

const EMPTY_DETAILS_STATE: EntityDatabaseDetailsState = {
  loading: false,
  error: null,
  data: null,
};

function areStringArraysEqual(left: string[], right: string[]): boolean {
  if (left.length !== right.length) {
    return false;
  }
  for (let index = 0; index < left.length; index += 1) {
    if (left[index] !== right[index]) {
      return false;
    }
  }
  return true;
}

function areLookupResponsesEqual(
  left: EntityInspectorLookupResponse,
  right: EntityInspectorLookupResponse
): boolean {
  if (
    left.entity.entityId !== right.entity.entityId ||
    left.entity.ownerId !== right.entity.ownerId ||
    left.entity.clientId !== right.entity.clientId ||
    left.entity.world !== right.entity.world ||
    left.truncated !== right.truncated ||
    left.totalMatches !== right.totalMatches
  ) {
    return false;
  }

  if (!areStringArraysEqual(left.terms, right.terms)) {
    return false;
  }

  if (left.sourceSummaries.length !== right.sourceSummaries.length) {
    return false;
  }
  for (let index = 0; index < left.sourceSummaries.length; index += 1) {
    const leftSummary = left.sourceSummaries[index];
    const rightSummary = right.sourceSummaries[index];
    if (
      leftSummary.id !== rightSummary.id ||
      leftSummary.name !== rightSummary.name ||
      leftSummary.matchCount !== rightSummary.matchCount
    ) {
      return false;
    }
  }

  if (left.records.length !== right.records.length) {
    return false;
  }
  for (let index = 0; index < left.records.length; index += 1) {
    const leftRecord = left.records[index];
    const rightRecord = right.records[index];
    if (
      leftRecord.source !== rightRecord.source ||
      leftRecord.key !== rightRecord.key ||
      leftRecord.type !== rightRecord.type ||
      leftRecord.entryCount !== rightRecord.entryCount ||
      leftRecord.ttlSeconds !== rightRecord.ttlSeconds ||
      leftRecord.payload !== rightRecord.payload ||
      leftRecord.matchedIn !== rightRecord.matchedIn ||
      leftRecord.relevance !== rightRecord.relevance ||
      !areStringArraysEqual(leftRecord.matchedTerms, rightRecord.matchedTerms)
    ) {
      return false;
    }
  }

  return true;
}

export function useEntityDatabaseDetails(
  entity: AuthorityEntityTelemetry | null,
  pollIntervalMs = 0
): EntityDatabaseDetailsState {
  const [state, setState] = useState<EntityDatabaseDetailsState>(EMPTY_DETAILS_STATE);
  const [frozenLookupContext, setFrozenLookupContext] = useState<LookupContext | null>(
    null
  );
  const entityId = entity?.entityId ?? '';
  const ownerId = entity?.ownerId ?? '';
  const clientId = entity?.clientId ?? '';
  const world = entity?.world ?? 0;
  const pollingEnabled = pollIntervalMs > 0 && pollIntervalMs < POLL_DISABLED_AT_MS;

  useEffect(() => {
    if (!entityId) {
      setFrozenLookupContext(null);
      return;
    }

    setFrozenLookupContext((previous) => {
      if (!previous || previous.entityId !== entityId) {
        return { entityId, ownerId, clientId, world };
      }

      if (
        pollingEnabled &&
        (previous.ownerId !== ownerId ||
          previous.clientId !== clientId ||
          previous.world !== world)
      ) {
        return { entityId, ownerId, clientId, world };
      }

      return previous;
    });
  }, [clientId, entityId, ownerId, pollingEnabled, world]);

  const lookupEntityId = entityId
    ? pollingEnabled
      ? entityId
      : (frozenLookupContext?.entityId ?? entityId)
    : '';
  const lookupOwnerId = entityId
    ? pollingEnabled
      ? ownerId
      : (frozenLookupContext?.ownerId ?? ownerId)
    : '';
  const lookupClientId = entityId
    ? pollingEnabled
      ? clientId
      : (frozenLookupContext?.clientId ?? clientId)
    : '';
  const lookupWorld = entityId
    ? pollingEnabled
      ? world
      : (frozenLookupContext?.world ?? world)
    : 0;

  useEffect(() => {
    if (!lookupEntityId) {
      setState(EMPTY_DETAILS_STATE);
      return;
    }

    const params = new URLSearchParams();
    params.set('entityId', lookupEntityId);
    params.set('ownerId', lookupOwnerId);
    params.set('clientId', lookupClientId);
    params.set('world', String(lookupWorld));

    const ac = new AbortController();
    setState((previous) => ({
      loading: previous.data?.entity.entityId !== lookupEntityId,
      error: null,
      data:
        previous.data?.entity.entityId === lookupEntityId ? previous.data : null,
    }));

    let inFlight = false;

    async function loadDetails() {
      if (inFlight) {
        return;
      }
      inFlight = true;
      try {
        const response = await fetch(
          `/api/map-entity-inspector?${params.toString()}`,
          {
            cache: 'no-store',
            signal: ac.signal,
          }
        );
        if (!response.ok) {
          setState((previous) => ({
            loading: false,
            error: `Entity database lookup failed (${response.status}).`,
            data: previous.data,
          }));
          return;
        }

        const payload = (await response.json()) as EntityInspectorLookupResponse;
        setState((previous) => {
          if (
            previous.loading === false &&
            previous.error == null &&
            previous.data != null &&
            areLookupResponsesEqual(previous.data, payload)
          ) {
            return previous;
          }
          return {
            loading: false,
            error: null,
            data: payload,
          };
        });
      } catch (error) {
        if (ac.signal.aborted) {
          return;
        }
        const message = error instanceof Error ? error.message : 'unknown error';
        setState((previous) => ({
          loading: false,
          error: `Entity database lookup failed (${message}).`,
          data: previous.data,
        }));
      } finally {
        inFlight = false;
      }
    }

    void loadDetails();

    const intervalId = pollingEnabled
      ? setInterval(() => {
          void loadDetails();
        }, pollIntervalMs)
      : null;

    return () => {
      ac.abort();
      if (intervalId != null) {
        clearInterval(intervalId);
      }
    };
  }, [
    lookupClientId,
    lookupEntityId,
    lookupOwnerId,
    lookupWorld,
    pollIntervalMs,
    pollingEnabled,
  ]);

  return state;
}
