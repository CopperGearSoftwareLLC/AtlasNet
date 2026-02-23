#pragma once
#include <chrono>
#include <sstream>
#include <thread>

#include "Global/Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Interlink/Interlink.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"

#ifndef NETWORK_MANIFEST_USE_PLAIN_STRING_DB
#define NETWORK_MANIFEST_USE_PLAIN_STRING_DB 1
#endif


class NetworkManifest : public Singleton<NetworkManifest> {
public:

    const std::string NetworkTelemetryTable = "Network_Telemetry";

	std::optional<NetworkIdentity> identifier;
	std::jthread HealthPingIntervalFunc;

	std::jthread HealthCheckOnFailureFunc;

public:
 void ScheduleNetworkPings();

 void TelemetryUpdate(const NetworkIdentity& identifier);

 //=================================
 //===          GET              ===
 //=================================
 void GetAllTelemetry(std::vector<std::vector<std::string>>& out_telemetry);
};


