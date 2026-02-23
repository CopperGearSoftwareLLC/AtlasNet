#include "AuthorityTelemetry.hpp"

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "InternalDB/InternalDB.hpp"

namespace
{
constexpr std::string_view kAuthorityTelemetryTable = "Authority_Telemetry";
constexpr size_t kAuthorityTelemetryColumnCount = 7;
constexpr size_t kAuthorityTelemetryRowWithEntityIdCount = 8;
}  // namespace

void AuthorityTelemetry::GetAllTelemetry(
	std::vector<std::vector<std::string>>& outTelemetry)
{
	outTelemetry.clear();
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
		outTelemetry.push_back(std::move(row));
	}
}
