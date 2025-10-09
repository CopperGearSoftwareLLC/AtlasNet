#include "pch.hpp"

int main()
{
  IDatabase* database = new RedisCacheDatabase(true);
  if (!database->Connect())
    return 0;

  database->Set("RedisCacheDatabase", "Database was here :D");
  std::string out = database->Get("foo");
  //std::cerr << out << std::endl;

  //database->Remove("foo");

  while (true)
  {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cerr << "||||||||||||||Database Enteries|||||||||||||||||" << std::endl;
    database->PrintEntireDB();
  }
  return 0;
}