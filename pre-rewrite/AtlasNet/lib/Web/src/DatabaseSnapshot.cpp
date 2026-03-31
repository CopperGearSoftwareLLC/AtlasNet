#include "DatabaseSnapshot.hpp"

#include <hiredis/read.h>

#include <algorithm>
#include <array>
#include <exception>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "InternalDB/InternalDB.hpp"

namespace
{
constexpr std::string_view kSourceName = "InternalDB";
constexpr size_t kScanCount = 200;
constexpr size_t kMaxPayloadChars = 32 * 1024;

std::string ReplyToString(const redisReply* reply)
{
	if (reply == nullptr || reply->str == nullptr || reply->len <= 0)
	{
		return {};
	}

	return std::string(reply->str, static_cast<size_t>(reply->len));
}

std::string TruncatePayload(const std::string& payload)
{
	if (payload.size() <= kMaxPayloadChars)
	{
		return payload;
	}

	std::ostringstream out;
	out << payload.substr(0, kMaxPayloadChars) << "\n...[truncated "
		<< (payload.size() - kMaxPayloadChars) << " bytes]";
	return out.str();
}

std::vector<std::string> ScanAllKeys()
{
	std::unordered_set<std::string> uniqueKeys;
	std::string cursor = "0";

	do
	{
		std::array<std::string, 4> scanCmd = {
			"SCAN", cursor, "COUNT", std::to_string(kScanCount)};

		auto reply = InternalDB::Get()->WithSync(
			[&](auto& redis) { return redis.command(scanCmd.begin(), scanCmd.end()); });

		if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements < 2)
		{
			break;
		}

		const redisReply* cursorReply = reply->element[0];
		if (cursorReply && cursorReply->type == REDIS_REPLY_STRING)
		{
			cursor = ReplyToString(cursorReply);
		}
		else
		{
			cursor = "0";
		}

		const redisReply* keysReply = reply->element[1];
		if (!keysReply || keysReply->type != REDIS_REPLY_ARRAY)
		{
			continue;
		}

		for (size_t i = 0; i < keysReply->elements; ++i)
		{
			const redisReply* keyReply = keysReply->element[i];
			if (!keyReply || keyReply->type != REDIS_REPLY_STRING)
			{
				continue;
			}

			const std::string key = ReplyToString(keyReply);
			if (!key.empty())
			{
				uniqueKeys.insert(key);
			}
		}
	}
	while (cursor != "0");

	std::vector<std::string> keys(uniqueKeys.begin(), uniqueKeys.end());
	std::sort(keys.begin(), keys.end());
	return keys;
}

std::string GetRedisKeyType(const std::string& key)
{
	std::array<std::string, 2> typeCmd = {"TYPE", key};
	auto reply = InternalDB::Get()->WithSync(
		[&](auto& redis) { return redis.command(typeCmd.begin(), typeCmd.end()); });

	if (!reply)
	{
		return "unknown";
	}

	if (reply->type != REDIS_REPLY_STRING && reply->type != REDIS_REPLY_STATUS)
	{
		return "unknown";
	}

	const std::string type = ReplyToString(reply.get());
	return type.empty() ? "unknown" : type;
}

std::vector<std::string> GetListValues(const std::string& key)
{
	std::array<std::string, 4> cmd = {"LRANGE", key, "0", "-1"};
	auto reply = InternalDB::Get()->WithSync(
		[&](auto& redis) { return redis.command(cmd.begin(), cmd.end()); });

	std::vector<std::string> values;
	if (!reply || reply->type != REDIS_REPLY_ARRAY)
	{
		return values;
	}

	values.reserve(reply->elements);
	for (size_t i = 0; i < reply->elements; ++i)
	{
		const redisReply* valueReply = reply->element[i];
		if (!valueReply || valueReply->type != REDIS_REPLY_STRING)
		{
			continue;
		}
		values.push_back(ReplyToString(valueReply));
	}

	return values;
}

std::vector<std::pair<std::string, std::string>> GetSortedSetValuesWithScores(
	const std::string& key)
{
	std::array<std::string, 5> cmd = {"ZRANGE", key, "0", "-1", "WITHSCORES"};
	auto reply = InternalDB::Get()->WithSync(
		[&](auto& redis) { return redis.command(cmd.begin(), cmd.end()); });

	std::vector<std::pair<std::string, std::string>> values;
	if (!reply || reply->type != REDIS_REPLY_ARRAY)
	{
		return values;
	}

	const size_t pairCount = reply->elements / 2;
	values.reserve(pairCount);

	for (size_t i = 0; i + 1 < reply->elements; i += 2)
	{
		const redisReply* memberReply = reply->element[i];
		const redisReply* scoreReply = reply->element[i + 1];
		if (!memberReply || !scoreReply || memberReply->type != REDIS_REPLY_STRING ||
			scoreReply->type != REDIS_REPLY_STRING)
		{
			continue;
		}

		values.emplace_back(ReplyToString(memberReply), ReplyToString(scoreReply));
	}

	return values;
}

std::optional<std::string> GetJsonValue(const std::string& key)
{
	return InternalDB::Get()->WithSync(
		[&](auto& redis) -> std::optional<std::string>
		{
			std::array<std::string, 3> cmd = {"JSON.GET", key, "."};
			return redis.template command<std::optional<std::string>>(cmd.begin(), cmd.end());
		});
}

std::string FormatHashPayload(
	const std::unordered_map<std::string, std::string>& fieldsAndValues)
{
	std::vector<std::pair<std::string, std::string>> sortedFields(
		fieldsAndValues.begin(), fieldsAndValues.end());
	std::sort(sortedFields.begin(), sortedFields.end());

	std::ostringstream out;
	for (const auto& [field, value] : sortedFields)
	{
		out << field << '\t' << value << '\n';
	}
	return out.str();
}

std::string FormatSetPayload(std::vector<std::string> members)
{
	std::sort(members.begin(), members.end());
	std::ostringstream out;
	for (const auto& member : members)
	{
		out << member << '\n';
	}
	return out.str();
}

std::string FormatListPayload(const std::vector<std::string>& values)
{
	std::ostringstream out;
	for (size_t i = 0; i < values.size(); ++i)
	{
		out << i << '\t' << values[i] << '\n';
	}
	return out.str();
}

std::string FormatSortedSetPayload(
	const std::vector<std::pair<std::string, std::string>>& values)
{
	std::ostringstream out;
	for (const auto& [member, score] : values)
	{
		out << score << '\t' << member << '\n';
	}
	return out.str();
}
}  // namespace

void DatabaseSnapshot::GetAllRows(std::vector<std::vector<std::string>>& outRows)
{
	outRows.clear();

	const std::vector<std::string> keys = ScanAllKeys();
	outRows.reserve(keys.size());

	for (const auto& key : keys)
	{
		std::string type = GetRedisKeyType(key);
		long long ttlSeconds = -2;
		long long entryCount = 0;
		std::string payload;

		try
		{
			ttlSeconds = InternalDB::Get()->TTL(key);

			if (type == "none")
			{
				payload = "<key expired>";
			}
			else if (type == "string")
			{
				const auto value = InternalDB::Get()->Get(key);
				payload = value.value_or("");
				entryCount = value.has_value() ? 1 : 0;
			}
			else if (type == "hash")
			{
				const auto fieldsAndValues = InternalDB::Get()->HGetAll(key);
				entryCount = static_cast<long long>(fieldsAndValues.size());
				payload = FormatHashPayload(fieldsAndValues);
			}
			else if (type == "set")
			{
				auto members = InternalDB::Get()->SMembers(key);
				entryCount = static_cast<long long>(members.size());
				payload = FormatSetPayload(std::move(members));
			}
			else if (type == "zset")
			{
				const auto members = GetSortedSetValuesWithScores(key);
				entryCount = static_cast<long long>(members.size());
				payload = FormatSortedSetPayload(members);
			}
			else if (type == "list")
			{
				const auto values = GetListValues(key);
				entryCount = static_cast<long long>(values.size());
				payload = FormatListPayload(values);
			}
			else if (type == "ReJSON-RL" || type == "json")
			{
				const auto jsonValue = GetJsonValue(key);
				payload = jsonValue.value_or("null");
				entryCount = (payload.empty() || payload == "null") ? 0 : 1;
			}
			else
			{
				payload = "<unsupported type>";
			}
		}
		catch (const std::exception& ex)
		{
			payload = std::string("<read error: ") + ex.what() + ">";
		}
		catch (...)
		{
			payload = "<read error>";
		}

		outRows.push_back(std::vector<std::string>{
			std::string(kSourceName),
			key,
			type,
			std::to_string(entryCount),
			std::to_string(ttlSeconds),
			TruncatePayload(payload)});
	}
}
