#include "Partition.hpp"
#include "Interlink/Interlink.hpp"
#include "pch.hpp"

Partition::Partition()
{
  // just for verifying partitions can connect with databases
	IDatabase* cacheDB = new RedisCacheDatabase();
  if (cacheDB && cacheDB->Connect())
  {
    // check write capability to database
      char hostname[128];
      gethostname(hostname, sizeof(hostname));
      cacheDB->Set(hostname, "Partition was here :D");
  }

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