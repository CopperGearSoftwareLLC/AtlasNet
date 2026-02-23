#include "NetworkManifest.hpp"
#include "Network/NetworkCredentials.hpp"
void NetworkManifest::ScheduleNetworkPings()
{
	//
	HealthPingIntervalFunc = std::jthread(
		[](std::stop_token st)
		{
			while (!st.stop_requested())
			{
				NetworkManifest::Get().TelemetryUpdate(NetworkCredentials::Get().GetID());
				std::this_thread::sleep_for(
					std::chrono::milliseconds(_NETWORK_TELEMETRY_PING_INTERVAL_MS));
			}
		});
}
void NetworkManifest::TelemetryUpdate(const NetworkIdentity& identifier)
{
	// Gather telemetry
	std::vector<ConnectionTelemetry> connections;
	Interlink::Get().GetConnectionTelemetry(connections);

	if (connections.empty())
	{
		// std::cout << "No connections found" << std::endl;
		return;
	}

	auto writeResult = int64_t(0);
#if NETWORK_MANIFEST_USE_PLAIN_STRING_DB
	// Debug-friendly mode: write both field and value as plain strings.
	const std::string shardId = identifier.ToString();
	std::ostringstream valueSS;
	for (const auto& telemetry : connections)
	{
		valueSS << telemetry.IdentityId << '\t' << telemetry.targetId << '\t' << telemetry.pingMs
				<< '\t' << telemetry.inBytesPerSec << '\t' << telemetry.outBytesPerSec << '\t'
				<< telemetry.inPacketsPerSec << '\t' << telemetry.pendingReliableBytes << '\t'
				<< telemetry.pendingUnreliableBytes << '\t' << telemetry.sentUnackedReliableBytes
				<< '\t' << telemetry.queueTimeUsec << '\t' << telemetry.qualityLocal << '\t'
				<< telemetry.qualityRemote << '\t' << telemetry.state << '\n';
	}

	writeResult = InternalDB::Get()->HSet(NetworkTelemetryTable, shardId, valueSS.str());
#else
	// Production mode: binary field/value for compact storage.
	ByteWriter valueBW;
	valueBW.u32(static_cast<uint32_t>(connections.size()));
	for (const auto& telemetry : connections)
	{
		telemetry.Serialize(valueBW);
	}

	ByteWriter fieldBW;
	fieldBW.uuid(identifier.ID);

	writeResult = InternalDB::Get()->HSet(NetworkTelemetryTable, fieldBW.as_string_view(),
										  valueBW.as_string_view());
#endif

	if (writeResult != 0)
	{
		std::printf("Failed to update network telemetry. HSET result: %lli\n",
					static_cast<long long>(writeResult));
	}
}

// Produces rows of telemetry, one row per connection.
// Column 0 is shardId (the Redis hash field), then ConnectionTelemetry fields after it.
void NetworkManifest::GetAllTelemetry(std::vector<std::vector<std::string>>& out_telemetry)
{
	out_telemetry.clear();

	const auto all = InternalDB::Get()->HGetAll(NetworkTelemetryTable);

	for (const auto& pair : all)
	{
#if NETWORK_MANIFEST_USE_PLAIN_STRING_DB
		const std::string shardId = pair.first;
		std::istringstream lines(pair.second);
		std::string line;
		while (std::getline(lines, line))
		{
			if (line.empty())
			{
				continue;
			}

			std::vector<std::string> columns;
			columns.reserve(13);
			std::string column;
			std::istringstream rowStream(line);
			while (std::getline(rowStream, column, '\t'))
			{
				columns.push_back(column);
			}

			if (columns.size() != 13)
			{
				continue;
			}

			std::vector<std::string> row;
			row.reserve(14);
			row.push_back(shardId);
			row.insert(row.end(), columns.begin(), columns.end());
			out_telemetry.push_back(std::move(row));
		}
#else
		// ============================
		// Deserialize FIELD (shard ID)
		// ============================
		ByteReader fieldBR(pair.first);
		std::string shardId;
		try
		{
			// Current format: raw UUID bytes.
			if (fieldBR.remaining() == 16)
			{
				NetworkIdentity shardIdentity = NetworkIdentity::MakeIDShard(fieldBR.uuid());
				shardId = shardIdentity.ToString();
			}
			else
			{
				// Legacy format fallback: length-prefixed string.
				shardId = fieldBR.str();
			}
		}
		catch (const std::exception&)
		{
			continue;
		}

		// ============================
		// Deserialize VALUE (connections)
		// ============================
		ByteReader valueBR(pair.second);
		const uint32_t count = valueBR.u32();

		for (uint32_t i = 0; i < count; ++i)
		{
			ConnectionTelemetry t;
			t.Deserialize(valueBR);

			std::vector<std::string> row;
			row.reserve(14);

			row.push_back(shardId);
			row.push_back(t.IdentityId);
			row.push_back(t.targetId);
			row.push_back(std::to_string(t.pingMs));
			row.push_back(std::to_string(t.inBytesPerSec));
			row.push_back(std::to_string(t.outBytesPerSec));
			row.push_back(std::to_string(t.inPacketsPerSec));
			row.push_back(std::to_string(t.pendingReliableBytes));
			row.push_back(std::to_string(t.pendingUnreliableBytes));
			row.push_back(std::to_string(t.sentUnackedReliableBytes));
			row.push_back(std::to_string(t.queueTimeUsec));
			row.push_back(std::to_string(t.qualityLocal));
			row.push_back(std::to_string(t.qualityRemote));
			row.push_back(std::to_string(t.state));

			// for (auto& field : row) {
			//     std::cerr << "  " << field << std::endl;
			// }

			out_telemetry.push_back(std::move(row));
		}
#endif
	}
}