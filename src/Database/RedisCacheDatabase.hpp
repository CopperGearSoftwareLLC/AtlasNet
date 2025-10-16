#pragma once
#include <string>
#include <optional>
#include <iostream>
#include <cstdlib>
#include "pch.hpp"
#include "IDatabase.hpp"

/**
 * @brief Database w/ Redis. Note: socket connections are TPC. fast, but partitions should still cache its own entity data
 * 
 */
class RedisCacheDatabase : public IDatabase {
public:
    RedisCacheDatabase(bool createDatabase = false, const std::string &host = "database-redis", int32 port = 6379, const std::string &network = "AtlasNet");

    bool Connect() override;
    std::future<bool> ConnectAsync() override;

    // --- String operations ---
    bool Set(const std::string &key, const std::string &value) override;
    std::string Get(const std::string &key) override;
    bool Remove(const std::string &key) override;
    bool Exists(const std::string &key) override;

        // --- Hash operations ---
    bool HashSet(const std::string& key, const std::string& field, const std::string& value) override;
    std::string HashGet(const std::string& key, const std::string& field) override;
    std::unordered_map<std::string, std::string> HashGetAll(const std::string& key) override;
    bool HashRemove(const std::string& key, const std::string& field) override;
    bool HashRemoveAll(const std::string& key) override;
    bool HashExists(const std::string& key, const std::string& field) override;

    void PrintEntireDB() override;
sw::redis::Redis& GetRaw()  {return *_redis;}
private:
    std::string _host;
    int32 _port;
    std::string _network;
    bool _autoStart;

    std::unique_ptr<sw::redis::Redis> _redis;
};
