#pragma once

#include <memory>
#include <atomic>

#include "Singleton.hpp"
#include "Debug/Log.hpp"
#include "Heuristic/Shape.hpp"
#include "Interlink/Connection.hpp"
#include "Interlink/InterlinkEnums.hpp"

class Partition : public Singleton<Partition>
{
	std::shared_ptr<Log> logger = std::make_shared<Log>("Partition");
	std::atomic_bool ShouldShutdown = false;
  public:
	Shape partitionShape;
	Partition();
	~Partition();
	void Init();
	void Shutdown() {ShouldShutdown = true;}
	void MessageArrived(const Connection &fromWhom, std::span<const std::byte> data);
};