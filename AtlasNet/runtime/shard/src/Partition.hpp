#pragma once
#include <atomic>

#include "Misc/Singleton.hpp"
#include "Log.hpp"
#include "Connection.hpp"


class Partition : public Singleton<Partition>
{
	std::shared_ptr<Log> logger = std::make_shared<Log>("Partition");
	std::atomic_bool ShouldShutdown = false;

  public:
	Partition();
	~Partition();
	void Init();
	void Shutdown() {ShouldShutdown = true;}
	void MessageArrived(const Connection &fromWhom, std::span<const std::byte> data);

private:
 
};