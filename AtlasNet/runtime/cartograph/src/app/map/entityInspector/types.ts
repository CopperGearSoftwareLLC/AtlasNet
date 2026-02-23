import type { DatabaseRecord } from '../../lib/cartographTypes';

export interface EntityInspectorMatchedRecord extends DatabaseRecord {
  matchedIn: 'key' | 'payload' | 'both';
  matchedTerms: string[];
  relevance: number;
}

export interface EntityInspectorSourceSummary {
  id: string;
  name: string;
  matchCount: number;
}

export interface EntityInspectorLookupResponse {
  entity: {
    entityId: string;
    ownerId: string;
    clientId: string;
    world: number;
  };
  searchedAt: string;
  terms: string[];
  truncated: boolean;
  totalMatches: number;
  sourceSummaries: EntityInspectorSourceSummary[];
  records: EntityInspectorMatchedRecord[];
}
