#include "pch.hpp"
#include "Database/RedisCacheDatabase.hpp"
#include "Database/ServerRegistry.hpp"
#include "Database/ProxyRegistry.hpp"
#include "Database/ShapeManifest.hpp"

int main()
{
  IDatabase* database = new RedisCacheDatabase(true);

  if (!database->Connect())
    return 0;
    
    std::cerr << "DATA WITHIN PERSISTENT VOLUME" << std::endl;
    database->PrintEntireDB();
    std::cerr << "=============================" << std::endl;
    
    database->Set("RedisCacheDatabase:String", "Database was here :D");
    database->HashSet("RedisCacheDatabase:Hash", "KeyFoo", "Database hash foo was here :D");
    database->HashSet("RedisCacheDatabase:Hash", "KeyBar", "Database hash bar was here :D");
    ServerRegistry::Get().ClearAll();
    ProxyRegistry::Get().ClearAll();
    ShapeManifest::Clear(database);

  while (true)
  {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cerr << "||||||||||||||Database Enteries|||||||||||||||||" << std::endl;
    database->PrintEntireDB();
  }
  return 0;
}