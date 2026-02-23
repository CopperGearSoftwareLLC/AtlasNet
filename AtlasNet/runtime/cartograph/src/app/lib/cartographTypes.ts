export interface Vec2 {
  x: number;
  y: number;
}

export interface ShapeJS {
  id?: string;
  ownerId?: string;
  type: 'circle' | 'rectangle' | 'polygon' | 'line' | 'rectImage';
  position: Vec2;
  radius?: number;
  size?: Vec2;
  points?: Vec2[];
  color?: string;
}

export interface ConnectionTelemetry {
  shardId?: string | null;
  IdentityId: string;
  targetId: string;
  pingMs: number;
  inBytesPerSec: number;
  outBytesPerSec: number;
  inPacketsPerSec: number;
  pendingReliableBytes: number;
  pendingUnreliableBytes: number;
  sentUnackedReliableBytes: number;
  queueTimeUsec: number;
  qualityLocal: number;
  qualityRemote: number;
  state: string;
}

export interface ShardTelemetry {
  shardId: string;
  downloadKbps: number;
  uploadKbps: number;
  connections: ConnectionTelemetry[];
}

export interface AuthorityEntityTelemetry {
  entityId: string;
  ownerId: string;
  world: number;
  x: number;
  y: number;
  z: number;
  isClient: boolean;
  clientId: string;
}

export interface DatabaseRecord {
  source: string;
  key: string;
  type: string;
  entryCount: number;
  ttlSeconds: number;
  payload: string;
}

export interface DatabaseSource {
  id: string;
  name: string;
  host: string;
  port: number;
  running: boolean;
  latencyMs: number | null;
}

export interface DatabaseSnapshotResponse {
  sources: DatabaseSource[];
  selectedSource: string | null;
  records: DatabaseRecord[];
}

export interface WorkerDaemonTelemetry {
  id: string;
  name: string;
  serverVersion: string;
  operatingSystem: string;
  osVersion: string;
  kernelVersion: string;
  architecture: string;
  cpuCount: number;
  memoryTotalBytes: number;
  containersTotal: number;
  containersRunning: number;
  containersPaused: number;
  containersStopped: number;
  swarmNodeId: string;
  swarmNodeAddress: string;
  swarmState: string;
  swarmControlAvailable: boolean;
}

export interface WorkerSwarmNodeTelemetry {
  id: string;
  hostname: string;
  status: string;
  availability: string;
  managerStatus: string | null;
  engineVersion: string;
  tlsStatus: string | null;
}

export interface WorkerContainerTelemetry {
  id: string;
  name: string;
  image: string;
  state: string;
  status: string;
  command: string;
  runningFor: string;
  createdAt: string;
  ports: string;
}

export interface WorkerContextTelemetry {
  name: string;
  description: string;
  dockerEndpoint: string;
  host: string | null;
  current: boolean;
  orchestrator: string | null;
  source: string | null;
  status: 'ok' | 'error';
  error: string | null;
  daemon: WorkerDaemonTelemetry | null;
  nodes: WorkerSwarmNodeTelemetry[];
  containers: WorkerContainerTelemetry[];
}

export interface WorkersSnapshotResponse {
  collectedAtMs: number | null;
  dockerCliAvailable: boolean;
  error: string | null;
  contexts: WorkerContextTelemetry[];
}
