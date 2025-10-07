#pragma once
#include "Singleton.hpp"
#include "Debug/Log.hpp"
#include "Database/IDatabase.hpp"

class Partition : public Singleton<Partition>
{
	std::shared_ptr<Log> logger = std::make_shared<Log>("Partition");

  public:
	Partition();
	~Partition();
	void Run();

  private:
  std::unique_ptr<IDatabase> _cacheDatabase;
  std::unique_ptr<IDatabase> _persistentDatabase;
};