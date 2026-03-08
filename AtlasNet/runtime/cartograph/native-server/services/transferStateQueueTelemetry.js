function toStringRows(telemetryVec) {
  const rows = [];
  for (let i = 0; i < telemetryVec.size(); i += 1) {
    const rowVec = telemetryVec.get(i);
    const row = [];
    for (let j = 0; j < rowVec.size(); j += 1) {
      row.push(String(rowVec.get(j)));
    }
    rows.push(row);
  }
  return rows;
}

function readTransferStateQueueTelemetry(addon, transferStateQueueView) {
  if (
    !addon ||
    !transferStateQueueView ||
    !addon.std_vector_std_vector_std_string__
  ) {
    return [];
  }

  const telemetryVec = new addon.std_vector_std_vector_std_string__();
  transferStateQueueView.DrainTransferStateQueue(telemetryVec);

  const rows = toStringRows(telemetryVec);
  const events = [];
  for (const row of rows) {
    if (!Array.isArray(row) || row.length < 6) {
      continue;
    }

    const transferId = String(row[0] || '').trim();
    const fromId = String(row[1] || '').trim();
    const toId = String(row[2] || '').trim();
    const stage = String(row[3] || '').trim() || 'eUnknown';
    const state = String(row[4] || '').trim() || 'source';
    const timestampMs = Number(row[5]);
    const entityIds = row
      .slice(6)
      .map((value) => String(value || '').trim())
      .filter((value) => value.length > 0);

    if (!transferId || !fromId || !toId || entityIds.length === 0) {
      continue;
    }

    events.push({
      transferId,
      fromId,
      toId,
      stage,
      state,
      entityIds,
      timestampMs: Number.isFinite(timestampMs) ? Math.floor(timestampMs) : Date.now(),
    });
  }

  return events;
}

module.exports = {
  readTransferStateQueueTelemetry,
};
