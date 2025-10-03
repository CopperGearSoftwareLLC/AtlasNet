#include "Partition.hpp"
#include "Interlink/Interlink.hpp"
#include "pch.hpp"
Partition::Partition()
{
	
}
Partition::~Partition()
{
	logger->Print("Goodbye from Partition!");
}
void Partition::Init()
{
Interlink::Get().Init(
		InterlinkProperties{.ThisID = InterLinkIdentifier(InterlinkType::ePartition,-1),
							.logger = logger,
							.bOpenListenSocket = true,
							.ListenSocketPort = CJ_LOCALHOST_PARTITION_PORT,
							.acceptConnectionFunc = [](const Connection &c) { return true; }});
	
	while (!ShouldShutdown)
	{

		Interlink::Get().Tick();

		
	}
	Interlink::Get().Shutdown();
}