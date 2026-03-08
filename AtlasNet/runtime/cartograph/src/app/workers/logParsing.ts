import type { WorkerContainerTelemetry } from '../shared/cartographTypes';

export function parseAggregateLogsBySource(aggregateLogs: string): Map<string, string> {
  const out = new Map<string, string>();
  const lines = String(aggregateLogs || '').split(/\r?\n/);
  const prefixedRows = [];

  let currentSource: string | null = null;
  let buffer: string[] = [];

  function flushCurrentSource() {
    if (!currentSource) {
      return;
    }
    const text = buffer.join('\n').trimEnd();
    out.set(currentSource, text);
  }

  for (const line of lines) {
    const sourceMatch = line.match(/^\[([^\]]+)\]\s*$/);
    if (sourceMatch) {
      flushCurrentSource();
      currentSource = sourceMatch[1].trim();
      buffer = [];
      continue;
    }

    const prefixedMatch = line.match(/^\[([^\]]+)\]\s?(.*)$/);
    if (prefixedMatch) {
      prefixedRows.push({
        source: prefixedMatch[1].trim(),
        line: String(prefixedMatch[2] ?? ''),
      });
      continue;
    }

    if (currentSource) {
      buffer.push(line);
    }
  }

  flushCurrentSource();
  if (out.size > 0 || prefixedRows.length === 0) {
    return out;
  }

  const bySource = new Map<string, string[]>();
  for (const row of prefixedRows) {
    if (!row.source) {
      continue;
    }
    if (!bySource.has(row.source)) {
      bySource.set(row.source, []);
    }
    bySource.get(row.source)?.push(row.line);
  }
  for (const [source, sourceLines] of bySource.entries()) {
    out.set(source, sourceLines.join('\n').trimEnd());
  }
  return out;
}

function collectContainerLogCandidates(container: WorkerContainerTelemetry): string[] {
  const values = new Set<string>();

  function addCandidate(value: string | null | undefined) {
    const text = String(value || '').trim();
    if (text) {
      values.add(text);
    }
  }

  addCandidate(container.name);
  addCandidate(container.id);

  const id = String(container.id || '').trim();
  const idWithoutPrefix = id.includes('://') ? id.split('://').pop() : null;
  addCandidate(idWithoutPrefix);

  const name = String(container.name || '').trim();
  if (name.includes('/')) {
    addCandidate(name.split('/').pop());
  }

  return Array.from(values.values());
}

export function resolveContainerLogs(
  container: WorkerContainerTelemetry,
  aggregateLogsBySource: Map<string, string>
): string {
  const directLogs = String(container.logs || '').trim();
  if (directLogs.length > 0) {
    return directLogs;
  }

  const candidates = collectContainerLogCandidates(container);
  for (const candidate of candidates) {
    if (aggregateLogsBySource.has(candidate)) {
      return aggregateLogsBySource.get(candidate) || '';
    }
  }

  for (const [source, text] of aggregateLogsBySource.entries()) {
    for (const candidate of candidates) {
      if (source.endsWith(`/${candidate}`)) {
        return text;
      }
    }
  }

  return '';
}
