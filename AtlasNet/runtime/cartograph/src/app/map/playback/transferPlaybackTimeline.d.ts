import type {
  TransferManifestTelemetry,
  TransferStateQueueTelemetry,
} from '../../shared/cartographTypes';

export interface ActiveTransferQueueEvent {
  event: TransferStateQueueTelemetry;
}

export interface IngestTransferQueueEventsArgs {
  events: TransferStateQueueTelemetry[] | null | undefined;
  nowMs: number;
  pollStartedAtMs: number;
  timeline: TransferStateQueueTelemetry[];
  lastTimestampByEntity: Map<string, number>;
  snapshotWindowMs: number;
  timestampSlotMs: number;
  maxEventAgeMs: number;
}

export interface IngestTransferQueueEventsResult {
  timeline: TransferStateQueueTelemetry[];
  lastTimestampByEntity: Map<string, number>;
  activeByEntity: Record<string, ActiveTransferQueueEvent>;
}

export interface SelectSnapshotTransferEventsArgs {
  timeline: TransferStateQueueTelemetry[];
  endMs: number;
  snapshotWindowMs: number;
}

export function normalizeTimestampMs(value: unknown, fallbackMs: number): number;
export function snapToNearestTickMs(valueMs: number, tickMs: number): number;
export function ingestTransferQueueEvents(
  args: IngestTransferQueueEventsArgs
): IngestTransferQueueEventsResult;
export function buildLiveTransferManifest(
  activeByEntity: Record<string, ActiveTransferQueueEvent>
): TransferManifestTelemetry[];
export function resolveTransferManifestAtCursor(
  events: TransferStateQueueTelemetry[],
  cursorMs: number,
  holdMs: number
): TransferManifestTelemetry[];
export function selectSnapshotTransferEvents(
  args: SelectSnapshotTransferEventsArgs
): TransferStateQueueTelemetry[];
