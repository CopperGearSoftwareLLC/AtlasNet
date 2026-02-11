#include "Partition.hpp"

#include <chrono>
#include <thread>
#include "Interlink.hpp"

#include "Misc/UUID.hpp"
#include "Packet/CommandPacket.hpp"
#include "pch.hpp"
#include "Database/HealthManifest.hpp"
#include "Telemetry/NetworkManifest.hpp"
Partition::Partition()
{

}
Partition::~Partition() {}

void Partition::Init()
{
	NetworkIdentity partitionIdentifier(NetworkIdentityType::eShard,
											UUIDGen::Gen());

	logger = std::make_shared<Log>(partitionIdentifier.ToString());
	HealthManifest::Get().ScheduleHealthPings(partitionIdentifier);
	NetworkManifest::Get().ScheduleNetworkPings(partitionIdentifier);
	Interlink::Get().Init(InterlinkProperties{
		.ThisID = partitionIdentifier,
		.logger = logger});

	// Clear any existing partition entity data to prevent stale data
	while (!ShouldShutdown)
	{
		
		std::this_thread::sleep_for(std::chrono::milliseconds(32));	 // ~30 updates per second
	}

	logger->Debug("Shutting Down");
	Interlink::Get().Shutdown();
}

void Partition::MessageArrived(const Connection &fromWhom, std::span<const std::byte> data)
{

}

