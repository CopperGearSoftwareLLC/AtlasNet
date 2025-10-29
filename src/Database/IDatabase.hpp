#pragma once
#include <pch.hpp>
#include <future>

class IDatabase {
public:
    virtual ~IDatabase() = default;

    /// Initialize connection, halts program until connection established
    virtual bool Connect() = 0;
    /// Initialize connection, returns immidiately
    /// must use .get() if you want to halt program until connection established
    virtual std::future<bool> ConnectAsync() = 0;


    /// Set key -> value. return true if success      
    virtual bool Set(const std::string& key, const std::string& value) = 0;

    // Get value for key (blank if missing)
    virtual std::string Get(const std::string& key) = 0;

    // Remove a data entry
    virtual bool Remove(const std::string& key) = 0;

    /// Check if key exists
    virtual bool Exists(const std::string& key) = 0;

    // hash functions
    /// set key -> field -> value
    /// Ex. HashSet(partition1:foo, bar, baz)
    virtual bool HashSet(const std::string& key, const std::string& field, const std::string& value) = 0;
    /// get value from key -> field 
    virtual std::string HashGet(const std::string& key, const std::string& field) = 0;
    /// get map of all hashes
    virtual std::unordered_map<std::string, std::string> HashGetAll(const std::string& key) = 0;
    /// remove a hash
    virtual bool HashRemove(const std::string& key, const std::string& field) = 0;
    virtual bool HashRemoveAll(const std::string& key) = 0;
    /// check if hash exists
    virtual bool HashExists(const std::string& key, const std::string& field) = 0;
    
    /* START LIST FUNCTIONS */
    // Should generally be using the async version, the non async is just there as an option.
    const static inline int LIST_END = 1;

    virtual std::optional<std::string> ListGetAtIndex(const std::string_view key, long long index) const = 0;
    virtual std::future<std::optional<std::string>> ListGetAtIndexAsync(const std::string_view key, long long index) const = 0;
    // Returns the length of the list
    virtual long long ListInsert(const std::string_view key, const bool before, const std::string_view pivot, const std::string_view val) = 0;
    virtual std::future<long long> ListInsertAsync(const std::string_view key, const bool before, const std::string_view pivot, const std::string_view val) = 0;
    virtual long long ListLength(const std::string_vew key) const = 0;
    virtual std::future<long long> ListLengthAsync(const std::string_vew key) const = 0;
    virtual std::optional<std::string> ListPop(const std::string_view key, const bool front) = 0
    virtual std::future<std::optional<std::string>> ListPopAsync(const std::string_view key, const bool front) = 0;
    virtual long long ListPush(const std::string_view key, const string_view val, const bool front) = 0;
    virtual std::future<long long> ListPushAsync(const std::string_view key, const string_view val, const bool front) = 0;
    // Only pushes if the list exists
    virtual long long ListPushx(const std::string_view key, const std::string_view val, const bool front) = 0;
    virtual std::future<long long> ListPushxAsync(const std::string_view key, const std::string_view val, const bool front) = 0;
    // Removes the first 'count' elements that match the 'val' value.
    virtual long long ListRemoveN(const std::string_view key, const long long count, const std::string_view val) = 0;
    virtual std::future<long long> ListRemoveNAsync(const std::string_view key, const long long count, const std::string_view val) = 0;
    virtual void ListSet(const std::string_view key, long long index, const std::string_view val) = 0;
    virtual std::future<void> ListSetAsync(const std::string_view key, long long index, const std::string_view val) = 0;
    virtual void ListTrim(const std::string_view key, long long start, long long stop) = 0;
    virtual std::future<void> ListTrimAsync(const std::string_view key, long long start, long long stop) = 0;
    // pops the last element of 'source' and pushes it to the front of 'destination'
    virtual std::optional<std::string> ListPopPush(const std::string_view source, const std::string_view destination) = 0;
    virtual std::future<std::optional<std::string>> ListPopPushAsync(const std::string_view source, const std::string_view destination) = 0;
    /* END LIST FUNCTIONS */

    virtual void PrintEntireDB() = 0;
};
