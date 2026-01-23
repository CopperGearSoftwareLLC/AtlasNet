#pragma once
#include <chrono>
#include <thread>

#include "Misc/Singleton.hpp"
#include "InterlinkIdentifier.hpp"
#include "Interlink.hpp"
#include "InternalDB.hpp"
#include "ConnectionTelemetry.hpp"
#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"



class NetworkManifest : public Singleton<NetworkManifest> {
public:

    const std::string NetworkTelemetryTable = "Network_Telemetry";

	std::optional<InterLinkIdentifier> identifier;
	std::jthread HealthPingIntervalFunc;

	std::jthread HealthCheckOnFailureFunc;

public:
	void ScheduleNetworkPings(const InterLinkIdentifier& id)
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

    void TelemetryUpdate(const InterLinkIdentifier& identifier)
	{
        std::string_view s_b, s_id;
        // set data from interlink
        std::vector<ConnectionTelemetry> out;
        Interlink::Get().GetConnectionTelemetry(out);
		ByteWriter bw;

        // testing with 1 entry
        if (out.size() > 0)
        {
            out[0].Serialize(bw);
            std::cerr << out[0].targetId << std::endl;
            std::cerr << out[0].pingMs << std::endl;
            std::cerr << out[0].inBytesPerSec << std::endl;
            std::cerr << out[0].outBytesPerSec << std::endl;
        }
        // may prepend connection count in future
        //for (const auto& t : out) {
        //    t.Serialize(bw);
        //    std::cerr << t.targetId << std::endl;
        //}
        s_b = bw.as_string_view();

        ByteWriter bw_id;
		bw_id.str(identifier.ID);
		s_id = bw_id.as_string_view();

        // write to internal db
        // needs to overwrite val using identity + target (concat?) as name
		const auto setResult = InternalDB::Get()->HSet(NetworkTelemetryTable, s_id, s_b);
		if (setResult != 0)
		{
			std::printf("Failed to update network telemetry in Network Manifest. HSET result: %lli",setResult);
		}
	
	}

    //=================================
	//===          GET              ===
	//=================================
	void GetAllTelemetry(std::vector<std::vector<std::string>>& out_telemetry);
};

// maps as target id to serialized telemetry data
inline void NetworkManifest::GetAllTelemetry(std::vector<std::vector<std::string>>& out_telemetry)
{
    ConnectionTelemetry telemetry;
    out_telemetry.clear();
    
    // assume 1 or none telemetry entry for now
    const auto All_Telemetry = InternalDB::Get()->HGetAll(NetworkTelemetryTable);

    for (const auto& pair : All_Telemetry)
    {
        ByteReader br(pair.second);
        telemetry.Deserialize(br);

        std::vector<std::string> telemetry_data;
        telemetry_data.push_back(telemetry.targetId);
        telemetry_data.push_back(std::to_string(telemetry.pingMs));
        telemetry_data.push_back(std::to_string(telemetry.inBytesPerSec));
        telemetry_data.push_back(std::to_string(telemetry.outBytesPerSec));
        telemetry_data.push_back(std::to_string(telemetry.inPacketsPerSec));
        telemetry_data.push_back(std::to_string(telemetry.pendingReliableBytes));
        telemetry_data.push_back(std::to_string(telemetry.pendingUnreliableBytes));
        telemetry_data.push_back(std::to_string(telemetry.sentUnackedReliableBytes));
        telemetry_data.push_back(std::to_string(telemetry.queueTimeUsec));
        telemetry_data.push_back(std::to_string(telemetry.qualityLocal));
        telemetry_data.push_back(std::to_string(telemetry.qualityRemote));
        telemetry_data.push_back(std::to_string(telemetry.state));

        out_telemetry.push_back(telemetry_data);
    }
}
