/**
 * @typedef {import('../../shared/cartographTypes').TransferManifestTelemetry} TransferManifestTelemetry
 * @typedef {import('../../shared/cartographTypes').TransferStateQueueTelemetry} TransferStateQueueTelemetry
 */

/**
 * @param {unknown} value
 * @param {number} fallbackMs
 * @returns {number}
 */
export function normalizeTimestampMs(value, fallbackMs) {
  const timestampMsRaw = Number(value);
  if (!Number.isFinite(timestampMsRaw) || timestampMsRaw <= 0) {
    return fallbackMs;
  }
  return Math.floor(timestampMsRaw);
}

/**
 * @param {number} valueMs
 * @param {number} tickMs
 * @returns {number}
 */
export function snapToNearestTickMs(valueMs, tickMs) {
  const safeTickMs = Math.max(1, Math.floor(tickMs));
  return Math.round(valueMs / safeTickMs) * safeTickMs;
}

/**
 * @param {{
 *   events: TransferStateQueueTelemetry[] | null | undefined;
 *   nowMs: number;
 *   pollStartedAtMs: number;
 *   timeline: TransferStateQueueTelemetry[];
 *   lastTimestampByEntity: Map<string, number>;
 *   snapshotWindowMs: number;
 *   timestampSlotMs: number;
 *   maxEventAgeMs: number;
 * }} args
 * @returns {{
 *   timeline: TransferStateQueueTelemetry[];
 *   lastTimestampByEntity: Map<string, number>;
 *   activeByEntity: Record<string, { event: TransferStateQueueTelemetry }>;
 * }}
 */
export function ingestTransferQueueEvents({
  events,
  nowMs,
  pollStartedAtMs,
  timeline,
  lastTimestampByEntity,
  snapshotWindowMs,
  timestampSlotMs,
  maxEventAgeMs,
}) {
  const queueEvents = Array.isArray(events) ? events : [];
  const nextTimeline = [...timeline];
  const nextLastTimestampByEntity = new Map(lastTimestampByEntity);
  const latestEventByEntity = new Map();
  /** @type {Record<string, { event: TransferStateQueueTelemetry }>} */
  const activeByEntity = {};

  for (const event of queueEvents) {
    const baseTimestampMs = normalizeTimestampMs(event?.timestampMs, nowMs);
    const snappedTimestampMs = snapToNearestTickMs(baseTimestampMs, timestampSlotMs);
    const entityIds = Array.isArray(event?.entityIds) ? event.entityIds : [];

    for (const rawEntityId of entityIds) {
      const entityId = String(rawEntityId || '').trim();
      if (!entityId) {
        continue;
      }

      const previousTimestampMs = nextLastTimestampByEntity.get(entityId) ?? -1;
      let adjustedTimestampMs = snappedTimestampMs;
      while (adjustedTimestampMs <= previousTimestampMs) {
        adjustedTimestampMs += timestampSlotMs;
      }
      nextLastTimestampByEntity.set(entityId, adjustedTimestampMs);

      const perEntityEvent = {
        ...event,
        entityIds: [entityId],
        timestampMs: adjustedTimestampMs,
      };
      nextTimeline.push(perEntityEvent);

      const currentLatest = latestEventByEntity.get(entityId);
      if (
        !currentLatest ||
        baseTimestampMs > currentLatest.sourceTimestampMs ||
        (baseTimestampMs === currentLatest.sourceTimestampMs &&
          perEntityEvent.timestampMs >= currentLatest.event.timestampMs)
      ) {
        latestEventByEntity.set(entityId, {
          event: perEntityEvent,
          sourceTimestampMs: baseTimestampMs,
        });
      }
    }
  }

  const cutoffMs = nowMs - snapshotWindowMs;
  while (nextTimeline.length > 0 && nextTimeline[0].timestampMs < cutoffMs) {
    nextTimeline.shift();
  }

  for (const [entityId, latest] of latestEventByEntity.entries()) {
    const ageMs = Math.max(0, pollStartedAtMs - latest.sourceTimestampMs);
    if (ageMs > maxEventAgeMs) {
      continue;
    }
    activeByEntity[entityId] = { event: latest.event };
  }

  return {
    timeline: nextTimeline,
    lastTimestampByEntity: nextLastTimestampByEntity,
    activeByEntity,
  };
}

/**
 * @param {Record<string, { event: TransferStateQueueTelemetry }>} activeByEntity
 * @returns {TransferManifestTelemetry[]}
 */
export function buildLiveTransferManifest(activeByEntity) {
  const rows = [];
  for (const [entityId, active] of Object.entries(activeByEntity)) {
    const event = active.event;
    rows.push({
      transferId: `${event.transferId}:${entityId}:${event.timestampMs}`,
      fromId: event.fromId,
      toId: event.toId,
      stage: event.stage,
      state: event.state,
      entityIds: [entityId],
      timestampMs: event.timestampMs,
    });
  }
  return rows;
}

/**
 * @param {TransferStateQueueTelemetry[]} events
 * @param {number} cursorMs
 * @param {number} holdMs
 * @returns {TransferManifestTelemetry[]}
 */
export function resolveTransferManifestAtCursor(events, cursorMs, holdMs) {
  if (!Array.isArray(events) || events.length === 0) {
    return [];
  }

  const minTimestampMs = cursorMs - Math.max(0, holdMs);
  const latestByEntity = new Map();

  for (const event of events) {
    if (!event) {
      continue;
    }

    const timestampMs = normalizeTimestampMs(event.timestampMs, 0);
    if (timestampMs <= 0) {
      continue;
    }
    if (timestampMs < minTimestampMs || timestampMs > cursorMs) {
      continue;
    }

    const entityIds = Array.isArray(event.entityIds) ? event.entityIds : [];
    for (const rawEntityId of entityIds) {
      const entityId = String(rawEntityId || '').trim();
      if (!entityId) {
        continue;
      }

      latestByEntity.set(entityId, {
        ...event,
        entityIds: [entityId],
        timestampMs,
      });
    }
  }

  const rows = [];
  for (const [entityId, event] of latestByEntity.entries()) {
    rows.push({
      transferId: `${event.transferId}:${entityId}:${event.timestampMs}`,
      fromId: event.fromId,
      toId: event.toId,
      stage: event.stage,
      state: event.state,
      entityIds: [entityId],
      timestampMs: event.timestampMs,
    });
  }

  return rows;
}

/**
 * @param {{
 *   timeline: TransferStateQueueTelemetry[];
 *   endMs: number;
 *   snapshotWindowMs: number;
 * }} args
 * @returns {TransferStateQueueTelemetry[]}
 */
export function selectSnapshotTransferEvents({ timeline, endMs, snapshotWindowMs }) {
  let transferEvents = timeline.filter(
    (event) => event.timestampMs >= endMs - snapshotWindowMs && event.timestampMs <= endMs
  );

  if (transferEvents.length === 0 && timeline.length > 0) {
    const latestEventTimestampMs = timeline[timeline.length - 1].timestampMs;
    transferEvents = timeline.filter(
      (event) => event.timestampMs >= latestEventTimestampMs - snapshotWindowMs
    );
  }

  return transferEvents;
}
