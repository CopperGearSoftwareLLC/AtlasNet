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
