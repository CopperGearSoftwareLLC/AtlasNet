#include "RedisConnection.hpp"

#include <hiredis/read.h>

#include <iostream>
#include <string_view>

long long RedisConnection::HSet(const std::string_view& key, const std::string_view& field,
								const std::string_view& value) const
{
	return WithSync([&](auto& r) -> long long { return r.hset(key, field, value); });
}

std::future<long long> RedisConnection::HSetAsync(const std::string_view& key,
												  const std::string_view& field,
												  const std::string_view& value) const
{
	return WithAsync([&](auto& r) -> std::future<long long> { return r.hset(key, field, value); });
}

std::optional<std::string> RedisConnection::HGet(const std::string_view& key,
												 const std::string_view& field) const
{
	return WithSync([&](auto& r) -> std::optional<std::string> { return r.hget(key, field); });
}

std::future<std::optional<std::string>> RedisConnection::HGetAsync(
	const std::string_view& key, const std::string_view& field) const
{
	return WithAsync([&](auto& r) -> std::future<std::optional<std::string>>
					 { return r.hget(key, field); });
}




bool RedisConnection::HExists(const std::string_view& key, const std::string_view& field) const
{
	return WithSync([&](auto& r) -> bool { return r.hexists(key, field); });
}

std::future<bool> RedisConnection::HExistsAsync(const std::string_view& key,
												const std::string_view& field) const
{
	return WithAsync([&](auto& r) -> std::future<bool> { return r.hexists(key, field); });
}

long long RedisConnection::HDel(const std::string_view& key,
								const std::vector<std::string_view>& fields) const
{
	return WithSync(
		[&](auto& r) -> long long
		{
			if (fields.empty())
				return 0LL;
			return r.hdel(key, fields.begin(), fields.end());
		});
}

std::future<long long> RedisConnection::HDelAsync(const std::string_view& key,
												  const std::vector<std::string_view>& fields) const
{
	return WithAsync(

		[&](auto& r) -> std::future<long long>
		{
			if (fields.empty())
				return r.template command<long long>(
					"ECHO", "0");  // cheap resolved future alternative is library-dependent
			return r.hdel(key, fields.begin(), fields.end());
		});
}

long long RedisConnection::HLen(const std::string_view& key) const
{
	return WithSync([&](auto& r) -> long long { return r.hlen(key); });
}

std::future<long long> RedisConnection::HLenAsync(const std::string_view& key) const
{
	return WithAsync([&](auto& r) -> std::future<long long> { return r.hlen(key); });
}

long long RedisConnection::HIncrBy(const std::string_view& key, const std::string_view& field,
								   long long by) const
{
	return WithSync([&](auto& r) -> long long { return r.hincrby(key, field, by); });
}

std::future<long long> RedisConnection::HIncrByAsync(const std::string_view& key,
													 const std::string_view& field,
													 long long by) const
{
	return WithAsync([&](auto& r) -> std::future<long long> { return r.hincrby(key, field, by); });
}

std::vector<std::optional<std::string>> RedisConnection::HMGet(
	const std::string_view& key, const std::vector<std::string_view>& fields) const
{
	return WithSync(
		[&](auto& r) -> std::vector<std::optional<std::string>>
		{
			std::vector<std::optional<std::string>> out;
			out.reserve(fields.size());
			if (!fields.empty())
			{
				r.hmget(key, fields.begin(), fields.end(), std::back_inserter(out));
			}
			return out;
		});
}

std::future<std::vector<std::optional<std::string>>> RedisConnection::HMGetAsync(
	const std::string_view& key, const std::vector<std::string_view>& fields) const
{
	return WithAsync(
		[&](auto& r) -> std::future<std::vector<std::optional<std::string>>>
		{
			// Many redis++ async methods can fill an output iterator (like
			// sync). If your async version doesn't support OutputIt, tell me
			// your redis++ version and I'll adapt this to a command()-based
			// parsing approach.
			std::vector<std::optional<std::string>> out;
			out.reserve(fields.size());

			// If async hmget(OutputIt) exists:
			return r.template hmget<std::vector<std::optional<std::string>>>(key, fields.begin(),
																			 fields.end());
		});
}
std::unordered_map<std::string, std::string> RedisConnection::HGetAll(
	const std::string_view& key) const
{
	std::unordered_map<std::string, std::string> out;

	WithSync(
		[&](auto& r)
		{
			// redis++ fills an output iterator of pairs
			r.hgetall(key, std::inserter(out, out.end()));
		});

	return out;
}
std::future<std::unordered_map<std::string, std::string>> RedisConnection::HGetAllAsync(
	const std::string_view& key) const
{
	return WithAsync(

		[&](auto& r) -> std::future<std::unordered_map<std::string, std::string>>
		{ return r.template hgetall<std::unordered_map<std::string, std::string>>(key); });
}
long long RedisConnection::DelKey(const std::string_view& key) const
{
	return WithSync([&](auto& r) -> long long { return r.del(key); });
}
std::future<long long> RedisConnection::DelKeyAsync(const std::string_view& key) const
{
	return WithAsync([&](auto& r) -> std::future<long long> { return r.del(key); });
}
std::optional<std::string> RedisConnection::Get(std::string_view key) const
{
	return WithSync([&](auto& r) { return r.get(key); });
}
std::future<std::optional<std::string>> RedisConnection::GetAsync(std::string_view key) const
{
	return WithAsync([&](auto& r) { return r.get(key); });
}
void RedisConnection::GetAsyncCb(std::string_view key, ResultCb<std::optional<std::string>> ok,
								 ErrorCb err) const
{
	FutureToCallback<std::optional<std::string>>(GetAsync(key), std::move(ok), std::move(err));
}
bool RedisConnection::Set(std::string_view key, std::string_view value) const
{
	return WithSync([&](auto& r) { return r.set(key, value); });
}
std::future<bool> RedisConnection::SetAsync(std::string_view key, std::string_view value) const
{
	return WithAsync([&](auto& r) { return r.set(key, value); });
}
void RedisConnection::SetAsyncCb(std::string_view key, std::string_view value, ResultCb<bool> ok,
								 ErrorCb err) const
{
	FutureToCallback<bool>(SetAsync(key, value), std::move(ok), std::move(err));
}
long long RedisConnection::ExistsCount(std::string_view key) const
{
	return WithSync([&](auto& r) { return r.exists(key); });
}
bool RedisConnection::Exists(std::string_view key) const
{
	return ExistsCount(key) != 0;
}
std::future<long long> RedisConnection::ExistsCountAsync(std::string_view key) const
{
	return WithAsync([&](auto& r) { return r.exists(key); });
}
std::future<bool> RedisConnection::ExistsAsync(std::string_view key) const
{
	return MapFuture<long long>(ExistsCountAsync(key), [](long long n) { return n != 0; });
}
void RedisConnection::ExistsAsyncCb(std::string_view key, ResultCb<bool> ok, ErrorCb err) const
{
	FutureToCallback<bool>(ExistsAsync(key), std::move(ok), std::move(err));
}
long long RedisConnection::Del(std::string_view key) const
{
	return WithSync([&](auto& r) { return r.del(key); });
}
std::future<long long> RedisConnection::DelAsync(std::string_view key) const
{
	return WithAsync([&](auto& r) { return r.del(key); });
}
void RedisConnection::DelAsyncCb(std::string_view key, ResultCb<long long> ok, ErrorCb err) const
{
	FutureToCallback<long long>(DelAsync(key), std::move(ok), std::move(err));
}
bool RedisConnection::Expire(std::string_view key, std::chrono::seconds ttl) const
{
	return WithSync([&](auto& r) { return r.expire(key, ttl); });
}
std::future<bool> RedisConnection::ExpireAsync(std::string_view key, std::chrono::seconds ttl) const
{
	return WithAsync([&](auto& r) { return r.expire(key, ttl); });
}
void RedisConnection::ExpireAsyncCb(std::string_view key, std::chrono::seconds ttl,
									ResultCb<bool> ok, ErrorCb err) const
{
	FutureToCallback<bool>(ExpireAsync(key, ttl), std::move(ok), std::move(err));
}
long long RedisConnection::TTL(std::string_view key) const
{
	return WithSync([&](auto& r) { return r.ttl(key); });
}
std::future<long long> RedisConnection::TTLAsync(std::string_view key) const
{
	return WithAsync([&](auto& r) { return r.ttl(key); });
}
void RedisConnection::TTLAsyncCb(std::string_view key, ResultCb<long long> ok, ErrorCb err) const
{
	FutureToCallback<long long>(TTLAsync(key), std::move(ok), std::move(err));
}

// =====================
// Sets
// =====================

long long RedisConnection::SAdd(const std::string_view& key,
								const std::vector<std::string_view>& members) const
{
	return WithSync(
		[&](auto& r) -> long long
		{
			if (members.empty())
				return 0LL;
			return r.sadd(key, members.begin(), members.end());
		});
}

std::future<long long> RedisConnection::SAddAsync(
	const std::string_view& key, const std::vector<std::string_view>& members) const
{
	return WithAsync(
		[&](auto& r) -> std::future<long long>
		{
			if (members.empty())
				return r.template command<long long>("ECHO", "0");
			return r.sadd(key, members.begin(), members.end());
		});
}

long long RedisConnection::SRem(const std::string_view& key,
								const std::vector<std::string_view>& members) const
{
	return WithSync(
		[&](auto& r) -> long long
		{
			if (members.empty())
				return 0LL;
			return r.srem(key, members.begin(), members.end());
		});
}

std::future<long long> RedisConnection::SRemAsync(
	const std::string_view& key, const std::vector<std::string_view>& members) const
{
	return WithAsync(
		[&](auto& r) -> std::future<long long>
		{
			if (members.empty())
				return r.template command<long long>("ECHO", "0");
			return r.srem(key, members.begin(), members.end());
		});
}

bool RedisConnection::SIsMember(const std::string_view& key, const std::string_view& member) const
{
	return WithSync([&](auto& r) -> bool { return r.sismember(key, member); });
}

std::future<bool> RedisConnection::SIsMemberAsync(const std::string_view& key,
												  const std::string_view& member) const
{
	return WithAsync([&](auto& r) -> std::future<bool> { return r.sismember(key, member); });
}

long long RedisConnection::SCard(const std::string_view& key) const
{
	return WithSync([&](auto& r) -> long long { return r.scard(key); });
}

std::future<long long> RedisConnection::SCardAsync(const std::string_view& key) const
{
	return WithAsync([&](auto& r) -> std::future<long long> { return r.scard(key); });
}

std::vector<std::string> RedisConnection::SMembers(const std::string_view& key) const
{
	return WithSync(
		[&](auto& r) -> std::vector<std::string>
		{
			std::vector<std::string> out;
			r.smembers(key, std::back_inserter(out));
			return out;
		});
}

std::future<std::vector<std::string>> RedisConnection::SMembersAsync(
	const std::string_view& key) const
{
	return WithAsync([&](auto& r) -> std::future<std::vector<std::string>>
					 { return r.template smembers<std::vector<std::string>>(key); });
}

// =====================
// Sorted Sets
// =====================

long long RedisConnection::ZAdd(const std::string_view& key, const std::string_view& member,
								double score) const
{
	return WithSync([&](auto& r) -> long long { return r.zadd(key, member, score); });
}

std::future<long long> RedisConnection::ZAddAsync(const std::string_view& key,
												  const std::string_view& member,
												  double score) const
{
	return WithAsync([&](auto& r) -> std::future<long long> { return r.zadd(key, member, score); });
}

long long RedisConnection::ZRem(const std::string_view& key, const std::string_view& member) const
{
	return WithSync([&](auto& r) -> long long { return r.zrem(key, member); });
}

std::future<long long> RedisConnection::ZRemAsync(const std::string_view& key,
												  const std::string_view& member) const
{
	return WithAsync([&](auto& r) -> std::future<long long> { return r.zrem(key, member); });
}

std::optional<double> RedisConnection::ZScore(const std::string_view& key,
											  const std::string_view& member) const
{
	return WithSync([&](auto& r) -> std::optional<double> { return r.zscore(key, member); });
}

std::future<std::optional<double>> RedisConnection::ZScoreAsync(
	const std::string_view& key, const std::string_view& member) const
{
	return WithAsync([&](auto& r) -> std::future<std::optional<double>>
					 { return r.zscore(key, member); });
}

std::vector<std::string> RedisConnection::ZRange(const std::string_view& key, long long start,
												 long long stop) const
{
	return WithSync(
		[&](auto& r) -> std::vector<std::string>
		{
			std::vector<std::string> out;
			r.zrange(key, start, stop, std::back_inserter(out));
			return out;
		});
}

std::future<std::vector<std::string>> RedisConnection::ZRangeAsync(const std::string_view& key,
																   long long start,
																   long long stop) const
{
	return WithAsync([&](auto& r) -> std::future<std::vector<std::string>>
					 { return r.template zrange<std::vector<std::string>>(key, start, stop); });
}

long long RedisConnection::ZCard(const std::string_view& key) const
{
	return WithSync([&](auto& r) -> long long { return r.zcard(key); });
}

std::future<long long> RedisConnection::ZCardAsync(const std::string_view& key) const
{
	return WithAsync([&](auto& r) -> std::future<long long> { return r.zcard(key); });
}
RedisConnection::RedisTime RedisConnection::GetTimeNow() const
{
	// Redis TIME command returns: [seconds, microseconds]
	const std::array<std::string, 1> args = {"TIME"};
	try
	{
		auto res = WithSync([&](auto& r) -> auto { return r.command(args.begin(), args.end()); });
		if (!res) {
    std::cerr << "GetTimeNow: Redis command returned nullptr" << std::endl;
    return RedisTime{};
}
		if (res->type != REDIS_REPLY_ARRAY)
		{
			std::cerr << "GetTimeNow Unknown reply of type " << res->type << std::endl;
			return RedisTime{};
		}
		const redisReply* secReply = res->element[0];
		int64_t seconds = 0;
		std::from_chars(secReply->str, secReply->str + secReply->len, seconds);
		int64_t microseconds = 0;
		if (res->elements >= 2)
		{
			const redisReply* usecReply = res->element[1];
			std::from_chars(usecReply->str, usecReply->str + usecReply->len, microseconds);
		}
		return RedisTime{seconds, microseconds};
	}
	catch (...)
	{
		return RedisTime{0, 0};
	}
}
double RedisConnection::GetTimeNowSeconds() const
{
	const auto t = GetTimeNow();
	return static_cast<double>(t.seconds) + static_cast<double>(t.microseconds) * 1e-6;
}
