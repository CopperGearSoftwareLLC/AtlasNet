import type { ShardHoverBounds } from './mapData';

export type ServerBoundsShardStatus = 'bounded' | 'unbounded' | 'bounded stale';

export interface ServerBoundsShardSummary {
  shardId: string;
  bounds: ShardHoverBounds | null;
  status: ServerBoundsShardStatus;
  hasClaimedBound: boolean;
  area: number;
  clientCount: number;
  entityCount: number;
  connectionCount: number;
  downloadKbps: number;
  uploadKbps: number;
}
