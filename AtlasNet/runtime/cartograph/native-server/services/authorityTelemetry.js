function readAuthorityTelemetry(addon, authorityTelemetry) {
  if (
    !addon ||
    !authorityTelemetry ||
    !addon.std_vector_std_vector_std_string__
  ) {
    return [];
  }

  const telemetryVec = new addon.std_vector_std_vector_std_string__();
  authorityTelemetry.GetAllTelemetry(telemetryVec);

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

module.exports = {
  readAuthorityTelemetry,
};
