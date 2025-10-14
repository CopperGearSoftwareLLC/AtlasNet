#include "Partition.hpp"
#include "Interlink/Interlink.hpp"
#include "pch.hpp"
#include "Database/IDatabase.hpp"
#include "Database/RedisCacheDatabase.hpp"
#include "Database/ServerRegistry.hpp"
Partition::Partition()
{
}
Partition::~Partition()
{
}
void Partition::Init()
{
	InterLinkIdentifier partitionIdentifier(InterlinkType::ePartition, DockerIO::Get().GetSelfContainerName());
	logger = std::make_shared<Log>(partitionIdentifier.ToString());
	Interlink::Get().Init(
		InterlinkProperties{
			.ThisID = partitionIdentifier,
			.logger = logger,
			.callbacks = {.acceptConnectionCallback = [](const Connection &c)
						  { return true; },
						  .OnConnectedCallback = [](const InterLinkIdentifier &Connection) {}}});


	std::string TestMessageStr = "Hello This is a test message from " + partitionIdentifier.ToString();
	
	Interlink::Get().SendMessageRaw(InterLinkIdentifier::MakeIDGod(),std::as_bytes(std::span(TestMessageStr)));
	// InterlinkMessage message;
	// message.SendTo(InterLinkIdentifier::MakeIDGod());
	// std::shared_ptr<dataPacket = std::make_shared<InterlinkDataPacket>();
	// Interlink::Get().SendMessage();
	while (!ShouldShutdown)
	{

		Interlink::Get().Tick();
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	logger->Debug("Shutting Down");
	Interlink::Get().Shutdown();
}