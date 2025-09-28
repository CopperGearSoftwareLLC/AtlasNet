#include "Partition.hpp"
#include "Interlink/Interlink.hpp"
#include "pch.hpp"
Partition::Partition()
{
	Interlink::Get().Initialize(
		InterlinkProperties{.Type = InterlinkType::ePartition,
							.logger = logger,
							.bOpenListenSocket = true,
							.ListenSocketPort = CJ_LOCALHOST_PARTITION_PORT,
							.acceptConnectionFunc = [](const Connection &c) { return true; }});
	Run();
}
Partition::~Partition()
{
	logger->Print("Goodbye from Partition!");
}
void Partition::Run()
{

	while (true)
	{

		Interlink::Get().Tick();

		std::this_thread::sleep_for(std::chrono::milliseconds(200));
	}
}