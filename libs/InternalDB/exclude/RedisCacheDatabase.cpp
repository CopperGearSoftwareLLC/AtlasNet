#include "RedisCacheDatabase.hpp"

RedisCacheDatabase::RedisCacheDatabase(bool createDatabase, const std::string &host,
                                       int32 port,
                                       const std::string &network)
    : _host(host), _port(port),
      _network(network),
      _autoStart(createDatabase)
{
    if (_autoStart)
    {
        // Ensure data directory exists (mounted volume target)
        std::system("mkdir -p /data");

        // Start Redis process locally (inside same container)
        std::string startCmd =
            "redis-server "
            "--appendonly yes "
            "--appendfilename appendonly.aof "
            "--save 1 1 "
            "--dir /data "
            "--protected-mode no "
            "--daemonize yes";

        int ret = std::system(startCmd.c_str());
        if (ret != 0)
        {
            std::cerr << "❌ Failed to start local Redis process.\n";
            return;
        }

        std::cerr << "🐳 Started Redis (persistent) on host " << _host
                  << ":" << _port << " with /data volume mount.\n";
    }
}

bool RedisCacheDatabase::Connect()
{
    auto future = ConnectAsync();

    // Wait up to 10 seconds (or customize)
    auto status = future.wait_for(std::chrono::seconds(10));
    if (status == std::future_status::ready)
    {
        return future.get();
    }

    std::cerr << "❌ Redis connection timed out.\n";
    return false;
}

std::future<bool> RedisCacheDatabase::ConnectAsync()
{
    using namespace std::chrono_literals;

    return std::async(std::launch::async, [this]() -> bool
                      {
        const int maxAttempts = 20;
        const int delayMs = 500;

        for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
            try {
                // Construct the Redis URI
                std::string uri = "tcp://" + _host + ":" + std::to_string(_port);

                // Try to connect
                _redis = std::make_unique<sw::redis::Redis>(uri);

                // Test the connection
                auto pong = _redis->ping();
                if (pong == "PONG") {
                    std::cerr << "✅ Connected to Redis at " << uri
                              << " on attempt " << attempt << ".\n";
                    return true;
                }

            } catch (const std::exception& e) {
                std::cerr << "⏳ Attempt " << attempt
                          << ": Redis not ready (" << e.what() << ")\n";
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }

        std::cerr << "❌ Failed to connect to Redis after " << maxAttempts << " attempts.\n";
        return false; });
}

bool RedisCacheDatabase::Set(const std::string &key, const std::string &value)
{
    if (!_redis)
        return false;
    return _redis->set(key, value);
}

std::string RedisCacheDatabase::Get(const std::string &key)
{
    if (!_redis)
        return "";

    auto opt = _redis->get(key);
    if (opt)
        return opt.value();
    else
        return "";
}

bool RedisCacheDatabase::Remove(const std::string &key)
{
    // Remove a single key
    if (!_redis)
        return false;
    return _redis->del(key);
}

bool RedisCacheDatabase::Exists(const std::string &key)
{
    if (!_redis)
        return false;
    return _redis->exists(key) > 0;
}

bool RedisCacheDatabase::HashSet(const std::string &key, const std::string &field, const std::string &value)
{
    if (!_redis)
        return false;
    try
    {
        return _redis->hset(key, field, value);
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Redis HashSet error for key: " << key << " field: " << field
                  << " (" << e.what() << ")\n";
        return false;
    }
}

std::string RedisCacheDatabase::HashGet(const std::string &key, const std::string &field)
{
    if (!_redis)
        return "";

    try
    {
        auto val = _redis->hget(key, field);
        if (val)
            return *val;
        return "";
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Redis HashGet error for key: " << key
                  << " field: " << field << " (" << e.what() << ")\n";
        return "";
    }
}

std::unordered_map<std::string, std::string> RedisCacheDatabase::HashGetAll(const std::string &key)
{
    ASSERT(_redis,"Invalid redis");

    try
    {
        std::unordered_map<std::string, std::string> result;
        _redis->hgetall(key, std::inserter(result, result.begin()));
        return result;
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Redis HashGetAll error for key: " << key
                  << " (" << e.what() << ")\n";
        return {};
    }
}

bool RedisCacheDatabase::HashRemove(const std::string &key, const std::string &field)
{
    if (!_redis)
        return false;

    try
    {
        return _redis->hdel(key, field) > 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Redis HashRemove error for key: " << key << " field: " << field
                  << " (" << e.what() << ")\n";
        return false;
    }
}

bool RedisCacheDatabase::HashRemoveAll(const std::string &key)
{
    if (!_redis)
        return false;

    try
    {
        // Using DEL to remove the entire hash key is faster than iterating over fields
        return _redis->del(key) > 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Redis HashRemoveAll error for key: " << key
                  << " (" << e.what() << ")\n";
        return false;
    }
}
bool RedisCacheDatabase::HashExists(const std::string &key, const std::string &field)
{
    if (!_redis)
        return false;

    try
    {
        return _redis->hexists(key, field);
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Redis HashExists error for key: " << key << " field: " << field
                  << " (" << e.what() << ")\n";
        return false;
    }
}

std::vector<std::string> RedisCacheDatabase::GetKeysMatching(const std::string &pattern)
{
    std::vector<std::string> keys;
    if (!_redis)
        return keys;

    try
    {
        // Use Redis KEYS command to find all keys matching the pattern
        // Note: KEYS is not recommended for production use due to performance,
        // but it's fine for cleanup operations during startup
        _redis->keys(pattern, std::back_inserter(keys));
    }
    catch (const std::exception &e)
    {
        std::cerr << "⚠️ Redis GetKeysMatching error for pattern: " << pattern
                  << " (" << e.what() << ")\n";
    }
    
    return keys;
}

void RedisCacheDatabase::PrintEntireDB()
{
    if (!_redis)
    {
        std::cerr << "❌ No Redis connection.\n";
        return;
    }

    long long cursor = 0;
    do
    {
        std::vector<std::string> keys;
        cursor = _redis->scan(cursor, "*", 100, std::back_inserter(keys));

        for (const auto &key : keys)
        {
            try
            {
                // Detect type
                auto type = _redis->type(key);

                if (type == "string")
                {
                    auto val = _redis->get(key);
                    std::cerr << key << " (string) = " << (val ? *val : "(nil)") << "\n";
                }
                else if (type == "hash")
                {
                    std::unordered_map<std::string, std::string> fields;
                    _redis->hgetall(key, std::inserter(fields, fields.begin()));
                    std::cerr << key << " (hash):\n";
                    for (auto &kv : fields)
                    {
                        std::cerr << "    " << kv.first << " = " << kv.second << "\n";
                    }
                }
                else if (type == "set")
                {
                    std::vector<std::string> members;
                    _redis->smembers(key, std::back_inserter(members));
                    std::cerr << key << " (set): { ";
                    for (auto &m : members)
                        std::cerr << m << " ";
                    std::cerr << "}\n";
                }
                else if (type == "list")
                {
                    std::vector<std::string> items;
                    _redis->lrange(key, 0, -1, std::back_inserter(items));
                    std::cerr << key << " (list): [ ";
                    for (auto &i : items)
                        std::cerr << i << " ";
                    std::cerr << "]\n";
                }
                else if (type == "zset")
                {
                }
                else
                {
                    std::cerr << key << " (unknown type: " << type << ")\n";
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "⚠️ Error reading key " << key << ": " << e.what() << "\n";
            }
        }
    } while (cursor != 0);
}