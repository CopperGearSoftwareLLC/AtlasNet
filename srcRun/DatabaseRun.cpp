#include "pch.hpp"
#include "Database/RedisCacheDatabase.hpp"

int main()
{
  IDatabase* database = new RedisCacheDatabase(true);
  if (database->Connect())
  {
    database->Set("test", "||||||||||||AddSomethingToDatabaseSuccess||||||||||||||");
    std::string out = database->Get("test").value();
    std::cerr << out << std::endl;
  }


  while (true)
  {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cerr << "working" << std::endl;
  }
  

  return 0;
}