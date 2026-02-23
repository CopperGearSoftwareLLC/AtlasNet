// SH telemetry bridge.
// Converts tracker DTO rows to manifest rows for persistence.

#include "SH_TelemetryPublisher.hpp"

#include <utility>
#include <vector>

#include "Entity/EntityHandoff/Telemetry/AuthorityManifest.hpp"
#include "SH_EntityAuthorityTracker.hpp"

namespace
{
std::vector<AuthorityManifest::TelemetryRow> ToManifestRows(
	const std::vector<SH_EntityAuthorityTracker::AuthorityTelemetryRow>& trackerRows)
{
	std::vector<AuthorityManifest::TelemetryRow> rows;
	rows.reserve(trackerRows.size());
	for (const auto& trackerRow : trackerRows)
	{
		AuthorityManifest::TelemetryRow row;
		row.entityId = trackerRow.entityId;
		row.owner = trackerRow.owner;
		row.entitySnapshot = trackerRow.entitySnapshot;
		row.world = trackerRow.world;
		row.position = trackerRow.position;
		row.isClient = trackerRow.isClient;
		row.clientId = trackerRow.clientId;
		rows.push_back(std::move(row));
	}
	return rows;
}
}  // namespace

void SH_TelemetryPublisher::PublishFromTracker(
	const SH_EntityAuthorityTracker& tracker) const
{
	std::vector<SH_EntityAuthorityTracker::AuthorityTelemetryRow> trackerRows;
	tracker.CollectTelemetryRows(trackerRows);
	AuthorityManifest::Get().TelemetryUpdate(ToManifestRows(trackerRows));
}
