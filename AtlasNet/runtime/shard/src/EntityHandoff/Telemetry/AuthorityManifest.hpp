#pragma once

// Authority telemetry persistence boundary used by shard runtime and later
// Cartograph overlays (minimal rows + full entity snapshots).

#include <format>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "EntityHandoff/EntityAuthorityTracker.hpp"
#include "InternalDB.hpp"
#include "Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Serialize/ByteWriter.hpp"

class AuthorityManifest : public Singleton<AuthorityManifest>
{
  public:
	static inline constexpr size_t kAuthorityTelemetryColumnCount = 7;
	static inline constexpr size_t kAuthorityTelemetryRowWithEntityIdCount = 8;

	void TelemetryUpdate(
		const std::vector<EntityAuthorityTracker::AuthorityTelemetryRow>& rows)
	{
		std::unordered_set<std::string> nextPublishedEntityFields;
		nextPublishedEntityFields.reserve(rows.size());
		for (const auto& row : rows)
		{
			const std::string field = std::to_string(row.entityId);
			const std::string value = std::format(
				"{}\t{}\t{}\t{}\t{}\t{}\t{}", row.owner.ToString(), row.world,
				row.position[0], row.position[1], row.position[2],
				row.isClient ? 1 : 0, row.clientId);
			const long long wrote =
				InternalDB::Get()->HSet(kAuthorityTelemetryTable, field, value);
			(void)wrote;

			ByteWriter snapshotWriter;
			row.owner.Serialize(snapshotWriter);
			row.entitySnapshot.Serialize(snapshotWriter);
			const std::string snapshotValue(snapshotWriter.as_string_view());
			const long long wroteSnapshot = InternalDB::Get()->HSet(
				kAuthorityEntitySnapshotTable, field, snapshotValue);
			(void)wroteSnapshot;
			nextPublishedEntityFields.insert(field);
		}

		std::vector<std::string> staleFields;
		for (const auto& oldField : lastPublishedEntityFields)
		{
			if (!nextPublishedEntityFields.contains(oldField))
			{
				staleFields.push_back(oldField);
			}
		}
		if (!staleFields.empty())
		{
			std::vector<std::string_view> staleFieldViews;
			staleFieldViews.reserve(staleFields.size());
			for (const auto& field : staleFields)
			{
				staleFieldViews.push_back(field);
			}
			const long long removed =
				InternalDB::Get()->HDel(kAuthorityTelemetryTable, staleFieldViews);
			(void)removed;
			const long long removedSnapshot = InternalDB::Get()->HDel(
				kAuthorityEntitySnapshotTable, staleFieldViews);
			(void)removedSnapshot;
		}
		lastPublishedEntityFields = std::move(nextPublishedEntityFields);
	}

	static void GetAllTelemetry(std::vector<std::vector<std::string>>& outRows)
	{
		outRows.clear();
		const auto all = InternalDB::Get()->HGetAll(kAuthorityTelemetryTable);
		for (const auto& [entityField, payload] : all)
		{
			std::vector<std::string> columns;
			columns.reserve(kAuthorityTelemetryColumnCount);
			std::istringstream rowStream(payload);
			std::string column;
			while (std::getline(rowStream, column, '\t'))
			{
				columns.push_back(column);
			}
			if (columns.size() != kAuthorityTelemetryColumnCount)
			{
				continue;
			}
			std::vector<std::string> row;
			row.reserve(kAuthorityTelemetryRowWithEntityIdCount);
			row.push_back(entityField);
			row.insert(row.end(), columns.begin(), columns.end());
			outRows.push_back(std::move(row));
		}
	}

	static std::unordered_map<std::string, std::string>
	GetAllEntitySnapshotsRaw()
	{
		return InternalDB::Get()->HGetAll(kAuthorityEntitySnapshotTable);
	}

  private:
	static inline constexpr std::string_view kAuthorityTelemetryTable =
		"Authority_Telemetry";
	static inline constexpr std::string_view kAuthorityEntitySnapshotTable =
		"Authority_EntitySnapshots";
	std::unordered_set<std::string> lastPublishedEntityFields;
};
