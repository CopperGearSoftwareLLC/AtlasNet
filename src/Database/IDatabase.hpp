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
    

    virtual void PrintEntireDB() = 0;
};
