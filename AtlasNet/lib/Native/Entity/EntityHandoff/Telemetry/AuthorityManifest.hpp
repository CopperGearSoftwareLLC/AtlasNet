#pragma once

// Authority telemetry persistence boundary used by shard runtime and later
// Cartograph overlays (minimal rows + full entity snapshots).

#include <format>
#include <chrono>
#include <sstream>
#include <string>
#include <string_view>
#include <optional>
#include <vector>

#include "Entity/EntityHandoff/EntityAuthorityTracker.hpp"
#include "Global/Misc/UUID.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Global/Serialize/ByteWriter.hpp"

class AuthorityManifest : public Singleton<AuthorityManifest>
{
  public:
	static inline constexpr size_t kAuthorityTelemetryColumnCount = 7;
	static inline constexpr size_t kAuthorityTelemetryRowWithEntityIdCount = 8;

	void TelemetryUpdate(
		const std::vector<EntityAuthorityTracker::AuthorityTelemetryRow>& rows)
	{
		for (const auto& row : rows)
		{
			const std::string field = std::to_string(row.entityId);
			const std::optional<std::string> previousTelemetryValue =
				InternalDB::Get()->HGet(kAuthorityTelemetryTable, field);
			std::string lastAuthority = row.owner.ToString();
			if (previousTelemetryValue.has_value())
			{
				const size_t ownerEnd = previousTelemetryValue->find('\t');
				if (ownerEnd != std::string::npos && ownerEnd > 0)
				{
					lastAuthority = previousTelemetryValue->substr(0, ownerEnd);
				}
			}

			const std::string value = std::format(
				"{}\t{}\t{}\t{}\t{}\t{}\t{}", row.owner.ToString(), row.world,
				row.position[0], row.position[1], row.position[2],
				row.isClient ? 1 : 0,  UUIDGen::encode_base20(row.clientId) );
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

			const auto nowMs =
				std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now().time_since_epoch())
					.count();
			const std::string entityMetaValue =
				std::format("{}\t{}", lastAuthority, nowMs);
			const long long wroteMeta = InternalDB::Get()->HSet(
				kAuthorityEntityMetaTable, field, entityMetaValue);
			(void)wroteMeta;
		}
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

	static std::unordered_map<std::string, std::string> GetAllEntityMetaRaw()
	{
		return InternalDB::Get()->HGetAll(kAuthorityEntityMetaTable);
	}

  private:
	static inline constexpr std::string_view kAuthorityTelemetryTable =
		"Authority_Telemetry";
	static inline constexpr std::string_view kAuthorityEntitySnapshotTable =
		"Authority_EntitySnapshots";
	static inline constexpr std::string_view kAuthorityEntityMetaTable =
		"Authority_EntityMeta";
};
