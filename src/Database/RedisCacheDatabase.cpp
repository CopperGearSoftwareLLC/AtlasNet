#include "RedisCacheDatabase.hpp"

RedisCacheDatabase::RedisCacheDatabase(bool createDatabase, const std::string &host,
                int32 port,
                const std::string &network)
      : _host(host), _port(port),
        _network(network),
        _autoStart(createDatabase) 
{
      if (_autoStart) {
    // Start Redis process locally (inside same container)
    std::string startCmd = "redis-server --appendonly yes --protected-mode no --daemonize yes";
    int ret = std::system(startCmd.c_str());
    if (ret != 0) {
        std::cerr << "❌ Failed to start local Redis process.\n";
        return;
    }
    std::cerr << "🐳 Started local Redis process on host + port " << _host << ":" << _port << "\n";
  }
}

bool RedisCacheDatabase::Connect()
{
    //if (_autoStart) {
  //    // Try to remove and restart Redis container (dev mode)
  //    std::string cmd =
  //        "docker rm -f " + _host + " >/dev/null 2>&1; "
  //        "docker run --network " + _network +
  //        " -d --name " + _host +
  //        " -p " + std::to_string(_port) + ":6379 redis:latest >/dev/null";
//
  //    int32 ret = std::system(cmd.c_str());
  //    if (ret != 0) {
  //        std::cerr << "❌ Failed to start Redis container. Command: " << cmd << "\n";
  //        return false;
  //    }
  //    std::cerr << "🐳 Started Redis container " << _host << " on port " << _port << "\n";
  //}

  try {
      std::string uri = "tcp://" + _host + ":" + std::to_string(_port);
      _redis = std::make_unique<sw::redis::Redis>(uri);
  } catch (const std::exception &e) {
      std::cerr << "❌ Redis connection failed: " << e.what() << "\n";
      return false;
  }
  std::cerr << "✅ DatabasePointer connected to " << _host << ":" << _port << "\n";
  return true;
}

bool RedisCacheDatabase::Set(const std::string &key, const std::string &value) {
    if (!_redis) return false;
    return _redis->set(key, value);
}

std::string RedisCacheDatabase::Get(const std::string &key) {
  if (!_redis) return "";

  auto opt = _redis->get(key);
  if (opt)
    return opt.value();
    else
    return "";
}

bool RedisCacheDatabase::Remove(const std::string &key) {
    // Remove a single key
    if (!_redis) return false;
    return _redis->del(key);
}

bool RedisCacheDatabase::Exists(const std::string &key) {
    if (!_redis) return false;
    return _redis->exists(key) > 0;
}

void RedisCacheDatabase::PrintEntireDB()
{
      if (!_redis) {
        std::cerr << "❌ No Redis connection.\n";
        return;
    }

    long long cursor = 0;
    do {
        std::vector<std::string> keys;
        cursor = _redis->scan(cursor, "*", 100, std::back_inserter(keys));

        for (const auto &key : keys) {
            try {
                // Detect type
                auto type = _redis->type(key);

                if (type == "string") {
                    auto val = _redis->get(key);
                    std::cerr << key << " (string) = " << (val ? *val : "(nil)") << "\n";

                } else if (type == "hash") {
                    std::unordered_map<std::string, std::string> fields;
                    _redis->hgetall(key, std::inserter(fields, fields.begin()));
                    std::cerr << key << " (hash):\n";
                    for (auto &kv : fields) {
                        std::cerr << "    " << kv.first << " = " << kv.second << "\n";
                    }

                } else if (type == "set") {
                    std::vector<std::string> members;
                    _redis->smembers(key, std::back_inserter(members));
                    std::cerr << key << " (set): { ";
                    for (auto &m : members) std::cerr << m << " ";
                    std::cerr << "}\n";

                } else if (type == "list") {
                    std::vector<std::string> items;
                    _redis->lrange(key, 0, -1, std::back_inserter(items));
                    std::cerr << key << " (list): [ ";
                    for (auto &i : items) std::cerr << i << " ";
                    std::cerr << "]\n";

                } 
                else if (type == "zset") {

                } else {
                    std::cerr << key << " (unknown type: " << type << ")\n";
                }

            } catch (const std::exception &e) {
                std::cerr << "⚠️ Error reading key " << key << ": " << e.what() << "\n";
            }
        }
    } while (cursor != 0);
}