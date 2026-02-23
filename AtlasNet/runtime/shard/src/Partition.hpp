#pragma once
#include <atomic>

#include "Global/Misc/Singleton.hpp"
#include "Debug/Log.hpp"
#include "Network/Connection.hpp"


class Partition : public Singleton<Partition>
{
	std::shared_ptr<Log> logger = std::make_shared<Log>("Partition");
	std::atomic_bool ShouldShutdown = false;

  public:
	Partition();
	~Partition();
	void Init();
	void Shutdown() {ShouldShutdown = true;}

private:
 
};