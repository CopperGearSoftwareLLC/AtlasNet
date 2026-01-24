export interface ConnectionTelemetry {
  connectionId: string;
  sourceShard: string;
  destShard: string;
  rttMs: number;
  packetLossPct: number;
  sendKbps: number;
  recvKbps: number;
  state: string;
}

export type TelemetryConnectionRow = string[];

export interface ShardTelemetry {
  shardId: string;
  downloadKbps: number;
  uploadKbps: number;

  connections: TelemetryConnectionRow[];
}
