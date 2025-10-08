#include "Partition.hpp"
#include "Interlink/Interlink.hpp"
#include "pch.hpp"

Partition::Partition()
{
  // just for verifying partitions can connect with databases
	IDatabase* cacheDB = new RedisCacheDatabase();
  if (cacheDB->Connect())
  {
      // Get current time as system clock
      auto now = std::chrono::system_clock::now();

      // Convert to time_t (seconds since epoch)
      std::time_t now_c = std::chrono::system_clock::to_time_t(now);

      // set the data in database
      cacheDB->Set(std::ctime(&now_c), "Partition's Data");
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