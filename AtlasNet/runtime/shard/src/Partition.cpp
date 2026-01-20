#include "Partition.hpp"

#include <chrono>
#include <thread>
#include "Interlink.hpp"

#include "Packet/CommandPacket.hpp"
#include "pch.hpp"
#include "Database/HealthManifest.hpp"
Partition::Partition()
{

}
Partition::~Partition() {}

void Partition::Init()
{
	InterLinkIdentifier partitionIdentifier(InterlinkType::eShard,
											DockerIO::Get().GetSelfContainerName());

	logger = std::make_shared<Log>(partitionIdentifier.ToString());
	HealthManifest::Get().ScheduleHealthPings(partitionIdentifier);
	Interlink::Get().Init(InterlinkProperties{
		.ThisID = partitionIdentifier,
		.logger = logger,
		.callbacks = {
			.acceptConnectionCallback = [](const Connection &c) { return true; },
			.OnConnectedCallback =
				[this](const InterLinkIdentifier &Connection)
			{
				
			},
			//.OnMessageArrival = [](const Connection &fromWhom, std::span<const std::byte> data)
			//{ },
			.OnDisconnectedCallback =
				[this](const InterLinkIdentifier &Connection)
			{
				
			}}});

	// Clear any existing partition entity data to prevent stale data
	while (!ShouldShutdown)
	{
		

		Interlink::Get().Tick();
		std::this_thread::sleep_for(std::chrono::milliseconds(32));	 // ~30 updates per second
	}

	logger->Debug("Shutting Down");
	Interlink::Get().Shutdown();
}

void Partition::MessageArrived(const Connection &fromWhom, std::span<const std::byte> data)
{

}

