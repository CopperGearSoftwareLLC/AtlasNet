#pragma once
#include <string>
#include <optional>

class IDatabase {
public:
    virtual ~IDatabase() = default;

    /// Initialize connection (or container in dev)
    virtual bool Connect() = 0;

    /// Set key -> value        
    virtual void Set(const std::string& key, const std::string& value) = 0;

    /// Get value for key (nullopt if missing)
    virtual std::optional<std::string> Get(const std::string& key) = 0;

    /// Check if key exists
    virtual bool Exists(const std::string& key) = 0;

    virtual void PrintEntireDB() = 0;
};
