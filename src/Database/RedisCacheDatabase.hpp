#pragma once
#include <string>
#include <optional>
#include <iostream>
#include <cstdlib>
#include "pch.hpp"
#include "IDatabase.hpp"

class RedisCacheDatabase : public IDatabase {
public:
    RedisCacheDatabase(bool createDatabase = false, const std::string &host = "database-redis",
                  int32 port = 6379,
                  const std::string &network = "AtlasNet")
        : _host(host), _port(port),
          _network(network),
          _autoStart(createDatabase) {}

    /**
     * Initializes connection to Redis.
     * If autoStart==true, will spin up container in dev mode.
     */
    bool Connect() override {
        if (_autoStart) {
            // Try to remove and restart Redis container (dev mode)
            std::string cmd =
                "docker rm -f " + _host + " >/dev/null 2>&1; "
                "docker run --network " + _network +
                " -d --name " + _host +
                " -p " + std::to_string(_port) + ":6379 redis:latest >/dev/null";

            int32 ret = std::system(cmd.c_str());
            if (ret != 0) {
                std::cerr << "❌ Failed to start Redis container. Command: " << cmd << "\n";
                return false;
            }
            std::cerr << "🐳 Started Redis container " << _host << " on port " << _port << "\n";
        }

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

    // --- Public interface ---

    void Set(const std::string &key, const std::string &value) override {
        if (_redis) _redis->set(key, value);
    }

    std::optional<std::string> Get(const std::string &key) override {
        if (!_redis) return std::nullopt;
        auto val = _redis->get(key);
        return val ? std::optional<std::string>(*val) : std::nullopt;
    }

    bool Exists(const std::string &key) override {
        if (!_redis) return false;
        return _redis->exists(key) > 0;
    }

    void PrintEntireDB() override {
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

private:
    std::string _host;
    int32 _port;
    std::string _network;
    bool _autoStart;

    std::unique_ptr<sw::redis::Redis> _redis;
};
