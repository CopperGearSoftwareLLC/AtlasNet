#include "RedisConnection.hpp"

long long RedisConnection::HSet(const std::string& key, const std::string& field,
								const std::string& value) const
{
	return WithSyncForKey(StringView{key},
						  [&](auto& r) -> long long { return r.hset(key, field, value); });
}

sw::redis::Future<long long> RedisConnection::HSetAsync(const std::string& key,
														const std::string& field,
														const std::string& value) const
{
	return WithAsyncForKey(StringView{key}, [&](auto& r) -> sw::redis::Future<long long>
						   { return r.hset(key, field, value); });
}

RedisConnection::OptionalString RedisConnection::HGet(const std::string& key,
													  const std::string& field) const
{
	return WithSyncForKey(StringView{key},
						  [&](auto& r) -> OptionalString { return r.hget(key, field); });
}

sw::redis::Future<RedisConnection::OptionalString> RedisConnection::HGetAsync(
	const std::string& key, const std::string& field) const
{
	return WithAsyncForKey(StringView{key}, [&](auto& r) -> sw::redis::Future<OptionalString>
						   { return r.hget(key, field); });
}

bool RedisConnection::HExists(const std::string& key, const std::string& field) const
{
	return WithSyncForKey(StringView{key}, [&](auto& r) -> bool { return r.hexists(key, field); });
}

sw::redis::Future<bool> RedisConnection::HExistsAsync(const std::string& key,
													  const std::string& field) const
{
	return WithAsyncForKey(
		StringView{key}, [&](auto& r) -> sw::redis::Future<bool> { return r.hexists(key, field); });
}

long long RedisConnection::HDel(const std::string& key,
								const std::vector<std::string>& fields) const
{
	return WithSyncForKey(StringView{key},
						  [&](auto& r) -> long long
						  {
							  if (fields.empty())
								  return 0LL;
							  return r.hdel(key, fields.begin(), fields.end());
						  });
}

sw::redis::Future<long long> RedisConnection::HDelAsync(
	const std::string& key, const std::vector<std::string>& fields) const
{
	return WithAsyncForKey(
		StringView{key},
		[&](auto& r) -> sw::redis::Future<long long>
		{
			if (fields.empty())
				return r.template command<long long>(
					"ECHO", "0");  // cheap resolved future alternative is library-dependent
			return r.hdel(key, fields.begin(), fields.end());
		});
}

long long RedisConnection::HLen(const std::string& key) const
{
	return WithSyncForKey(StringView{key}, [&](auto& r) -> long long { return r.hlen(key); });
}

sw::redis::Future<long long> RedisConnection::HLenAsync(const std::string& key) const
{
	return WithAsyncForKey(StringView{key},
						   [&](auto& r) -> sw::redis::Future<long long> { return r.hlen(key); });
}

long long RedisConnection::HIncrBy(const std::string& key, const std::string& field,
								   long long by) const
{
	return WithSyncForKey(StringView{key},
						  [&](auto& r) -> long long { return r.hincrby(key, field, by); });
}

sw::redis::Future<long long> RedisConnection::HIncrByAsync(const std::string& key,
														   const std::string& field,
														   long long by) const
{
	return WithAsyncForKey(StringView{key}, [&](auto& r) -> sw::redis::Future<long long>
						   { return r.hincrby(key, field, by); });
}

std::vector<RedisConnection::OptionalString> RedisConnection::HMGet(
	const std::string& key, const std::vector<std::string>& fields) const
{
	return WithSyncForKey(StringView{key},
						  [&](auto& r) -> std::vector<OptionalString>
						  {
							  std::vector<OptionalString> out;
							  out.reserve(fields.size());
							  if (!fields.empty())
							  {
								  r.hmget(key, fields.begin(), fields.end(),
										  std::back_inserter(out));
							  }
							  return out;
						  });
}

sw::redis::Future<std::vector<RedisConnection::OptionalString>> RedisConnection::HMGetAsync(
	const std::string& key, const std::vector<std::string>& fields) const
{
	return WithAsyncForKey(StringView{key},
						   [&](auto& r) -> sw::redis::Future<std::vector<OptionalString>>
						   {
							   // Many redis++ async methods can fill an output iterator (like
							   // sync). If your async version doesn't support OutputIt, tell me
							   // your redis++ version and I'll adapt this to a command()-based
							   // parsing approach.
							   std::vector<OptionalString> out;
							   out.reserve(fields.size());

							   // If async hmget(OutputIt) exists:
							   return r.template hmget<std::vector<OptionalString>>(
								   key, fields.begin(), fields.end());
						   });
}
std::unordered_map<std::string, std::string> RedisConnection::HGetAll(const std::string& key) const
{
	std::unordered_map<std::string, std::string> out;

	WithSyncForKey(StringView{key},
				   [&](auto& r)
				   {
					   // redis++ fills an output iterator of pairs
					   r.hgetall(key, std::inserter(out, out.end()));
				   });

	return out;
}
sw::redis::Future<std::unordered_map<std::string, std::string>> RedisConnection::HGetAllAsync(
	const std::string& key) const
{
	return WithAsyncForKey(
		StringView{key},
		[&](auto& r) -> sw::redis::Future<std::unordered_map<std::string, std::string>>
		{ return r.template hgetall<std::unordered_map<std::string, std::string>>(key); });
}
long long RedisConnection::DelKey(const std::string& key) const
{
	return WithSyncForKey(StringView{key}, [&](auto& r) -> long long { return r.del(key); });
}
sw::redis::Future<long long> RedisConnection::DelKeyAsync(const std::string& key) const
{
	return WithAsyncForKey(StringView{key},
						   [&](auto& r) -> sw::redis::Future<long long> { return r.del(key); });
}
RedisConnection::OptionalString RedisConnection::Get(StringView key) const
{
	return WithSyncForKey(key, [&](auto& r) { return r.get(key); });
}
RedisConnection::Future<RedisConnection::OptionalString> RedisConnection::GetAsync(
	StringView key) const
{
	return WithAsyncForKey(key, [&](auto& r) { return r.get(key); });
}
void RedisConnection::GetAsyncCb(StringView key, ResultCb<OptionalString> ok, ErrorCb err) const
{
	FutureToCallback<OptionalString>(GetAsync(key), std::move(ok), std::move(err));
}
bool RedisConnection::Set(StringView key, StringView value) const
{
	return WithSyncForKey(key, [&](auto& r) { return r.set(key, value); });
}
RedisConnection::Future<bool> RedisConnection::SetAsync(StringView key, StringView value) const
{
	return WithAsyncForKey(key, [&](auto& r) { return r.set(key, value); });
}
void RedisConnection::SetAsyncCb(StringView key, StringView value, ResultCb<bool> ok,
								 ErrorCb err) const
{
	FutureToCallback<bool>(SetAsync(key, value), std::move(ok), std::move(err));
}
long long RedisConnection::ExistsCount(StringView key) const
{
	return WithSyncForKey(key, [&](auto& r) { return r.exists(key); });
}
bool RedisConnection::Exists(StringView key) const
{
	return ExistsCount(key) != 0;
}
RedisConnection::Future<long long> RedisConnection::ExistsCountAsync(StringView key) const
{
	return WithAsyncForKey(key, [&](auto& r) { return r.exists(key); });
}
RedisConnection::Future<bool> RedisConnection::ExistsAsync(StringView key) const
{
	return MapFuture<long long>(ExistsCountAsync(key), [](long long n) { return n != 0; });
}
void RedisConnection::ExistsAsyncCb(StringView key, ResultCb<bool> ok, ErrorCb err) const
{
	FutureToCallback<bool>(ExistsAsync(key), std::move(ok), std::move(err));
}
long long RedisConnection::Del(StringView key) const
{
	return WithSyncForKey(key, [&](auto& r) { return r.del(key); });
}
RedisConnection::Future<long long> RedisConnection::DelAsync(StringView key) const
{
	return WithAsyncForKey(key, [&](auto& r) { return r.del(key); });
}
void RedisConnection::DelAsyncCb(StringView key, ResultCb<long long> ok, ErrorCb err) const
{
	FutureToCallback<long long>(DelAsync(key), std::move(ok), std::move(err));
}
bool RedisConnection::Expire(StringView key, std::chrono::seconds ttl) const
{
	return WithSyncForKey(key, [&](auto& r) { return r.expire(key, ttl); });
}
RedisConnection::Future<bool> RedisConnection::ExpireAsync(StringView key,
														   std::chrono::seconds ttl) const
{
	return WithAsyncForKey(key, [&](auto& r) { return r.expire(key, ttl); });
}
void RedisConnection::ExpireAsyncCb(StringView key, std::chrono::seconds ttl, ResultCb<bool> ok,
									ErrorCb err) const
{
	FutureToCallback<bool>(ExpireAsync(key, ttl), std::move(ok), std::move(err));
}
long long RedisConnection::TTL(StringView key) const
{
	return WithSyncForKey(key, [&](auto& r) { return r.ttl(key); });
}
RedisConnection::Future<long long> RedisConnection::TTLAsync(StringView key) const
{
	return WithAsyncForKey(key, [&](auto& r) { return r.ttl(key); });
}
void RedisConnection::TTLAsyncCb(StringView key, ResultCb<long long> ok, ErrorCb err) const
{
	FutureToCallback<long long>(TTLAsync(key), std::move(ok), std::move(err));
}

