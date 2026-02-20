export interface DatabaseRecord {
  source: string;
  key: string;
  type: string;
  entryCount: number;
  ttlSeconds: number;
  payload: string;
}
