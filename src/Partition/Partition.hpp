#pragma once
#include "Singleton.hpp"
#include "Debug/Log.hpp"
class Partition : public Singleton<Partition>
{
	std::shared_ptr<Log> logger = std::make_shared<Log>("Partition");

  public:
	Partition();
	~Partition();
	void Run();
};