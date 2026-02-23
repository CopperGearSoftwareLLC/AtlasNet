#pragma once

#include <string>
#include <vector>

class DatabaseSnapshot
{
   public:
	DatabaseSnapshot() = default;

	// Row schema:
	// [source, key, type, entryCount, ttlSeconds, payload]
	void GetAllRows(std::vector<std::vector<std::string>>& outRows);
};
