#pragma once
#include <chrono>
#include <thread>

#include "Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Interlink.hpp"
#include "InternalDB.hpp"
#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"



class NetworkManifest : public Singleton<NetworkManifest> {
public:

    const std::string NetworkTelemetryTable = "Network_Telemetry";

	std::optional<NetworkIdentity> identifier;
	std::jthread HealthPingIntervalFunc;

	std::jthread HealthCheckOnFailureFunc;

public:
	void ScheduleNetworkPings(const NetworkIdentity& id)
	{
        //
		HealthPingIntervalFunc = std::jthread(
			[ id = id](std::stop_token st)
			{
				while (!st.stop_requested())
				{
					NetworkManifest::Get().TelemetryUpdate(id);
					std::this_thread::sleep_for(
						std::chrono::milliseconds(_NETWORK_TELEMETRY_PING_INTERVAL_MS));
				}
			});
	}

void TelemetryUpdate(const NetworkIdentity& identifier)
{
    // Gather telemetry
    std::vector<ConnectionTelemetry> connections;
    Interlink::Get().GetConnectionTelemetry(connections);

    if (connections.empty())
    {
        //std::cout << "No connections found" << std::endl;
        return;
    }

    // ============================
    // Serialize VALUE (connections)
    // ============================
    ByteWriter valueBW;
    valueBW.u32(static_cast<uint32_t>(connections.size()));
    for (const auto& t : connections)
    {
        t.Serialize(valueBW);

        // For debugging
        //t.DebugLogs();
    }

    // ============================
    // Serialize FIELD (shard ID)
    // ============================
    ByteWriter fieldBW;
    fieldBW.uuid(identifier.ID);

    // ============================
    // Write to Redis (binary-safe)
    // ============================
    const auto result = InternalDB::Get()->HSet(
        NetworkTelemetryTable,
        fieldBW.as_string_view(),   // binary field
        valueBW.as_string_view()    // binary value
    );

    if (result != 0)
    {
        std::printf(
            "Failed to update network telemetry. HSET result: %lli\n",
            result
        );
    }
}

    //=================================
	//===          GET              ===
	//=================================
	void GetAllTelemetry(std::vector<std::vector<std::string>>& out_telemetry);
};

// Produces rows of telemetry, one row per connection.
// Column 0 is shardId (the Redis hash field), then ConnectionTelemetry fields after it.
void NetworkManifest::GetAllTelemetry(
    std::vector<std::vector<std::string>>& out_telemetry)
{
    out_telemetry.clear();

    const auto all = InternalDB::Get()->HGetAll(NetworkTelemetryTable);

    for (const auto& pair : all)
    {
        // ============================
        // Deserialize FIELD (shard ID)
        // ============================
        ByteReader fieldBR(pair.first);
        std::string shardId = fieldBR.str();

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
            row.reserve(13);

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

            //for (auto& field : row) {
            //    std::cerr << "  " << field << std::endl;
            //}

            out_telemetry.push_back(std::move(row));
        }
    }
}
