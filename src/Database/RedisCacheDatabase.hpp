#pragma once
#include <string>
#include <string_view>
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

    // --- Start List operations ---
    std::optional<std::string> ListGetAtIndex(const std::string_view key, long long index) const override;
    std::future<std::optional<std::string>> ListGetAtIndexAsync(const std::string_view key, long long index) const override;
    // returns the length of the list;
    long long ListInsert(const std::string_view key, const bool before, const std::string_view pivot, const std::string_view val) override;
    std::future<long long> ListInsertAsync(const std::string_view key, const bool before, const std::string_view pivot, const std::string_view val) override;
    long long ListLength(const std::string_vew key) const override;
    std::future<long long> ListLengthAsync(const std::string_vew key) const override;
    std::optional<std::string> ListPop(const std::string_view key, const bool front) override;
    std::future<std::optional<std::string>> ListPopAsync(const std::string_view key, const bool front) override;
    long long ListPush(const std::string_view key, const string_view val, const bool front) override;
    std::future<long long> ListPushAsync(const std::string_view key, const string_view val, const bool front) override;
    // only pushes if the list exists;
    long long ListPushx(const std::string_view key, const std::string_view val, const bool front) override;
    std::future<long long> ListPushxAsync(const std::string_view key, const std::string_view val, const bool front) override;
    // removes the first 'count' elements that match the 'val' value;
    long long ListRemoveN(const std::string_view key, const long long count, const std::string_view val) override;
    std::future<long long> ListRemoveNAsync(const std::string_view key, const long long count, const std::string_view val) override;
    void ListSet(const std::string_view key, long long index, const std::string_view val) override;
    std::future<void> ListSetAsync(const std::string_view key, long long index, const std::string_view val) override;
    void ListTrim(const std::string_view key, long long start, long long stop) override;
    std::future<void> ListTrimAsync(const std::string_view key, long long start, long long stop) override;
    // pops the last element of 'source' and pushes it to the front of 'destination'
    std::optional<std::string> ListPopPush(const std::string_view source, const std::string_view destination) override;
    std::future<std::optional<std::string>> ListPopPushAsync(const std::string_view source, const std::string_view destination) override;
    // --- End List Operations ---


    void PrintEntireDB() override;
sw::redis::Redis& GetRaw()  {return *_redis;}
private:
    std::string _host;
    int32 _port;
    std::string _network;
    bool _autoStart;

    std::unique_ptr<sw::redis::Redis> _redis;
};
