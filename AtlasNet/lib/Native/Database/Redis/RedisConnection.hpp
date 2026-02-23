#pragma once
#include <sw/redis++/async_redis++.h>
#include <sw/redis++/async_redis.h>
#include <sw/redis++/redis++.h>
#include <sw/redis++/redis.h>
#include <sw/redis++/subscriber.h>

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class RedisConnection
{
   public:
	/** Callback invoked with a successfully produced result. */
	template <class T>
	using ResultCb = std::function<void(T)>;

	/** Callback invoked if a std::future throws. */
	using ErrorCb = std::function<void(std::exception_ptr)>;

   private:
	bool IsCluster = false;

	// Sync
	std::unique_ptr<sw::redis::Redis> Handle;
	std::unique_ptr<sw::redis::RedisCluster> HandleCluster;

	// Async
	std::unique_ptr<sw::redis::AsyncRedis> HandleAsync;
	std::unique_ptr<sw::redis::AsyncRedisCluster> HandleAsyncCluster;

	

	template <class InT, class Func>
	static auto MapFuture(std::future<InT>&& fut, Func&& fn)
		-> std::future<std::invoke_result_t<Func, InT>>
	{
		using OutT = std::invoke_result_t<Func, InT>;
		// std::future has no continuation; we create one via std::async.
		return std::async(
			std::launch::async,
			[f = std::move(fut), fn = std::forward<Func>(fn)]() mutable -> OutT
			{ return fn(f.get()); });
	}

	template <class T>
	static void FutureToCallback(std::future<T>&& fut, ResultCb<T> ok,
								 ErrorCb err = {})
	{
		std::thread(
			[f = std::move(fut), ok = std::move(ok),
			 err = std::move(err)]() mutable
			{
				try
				{
					ok(f.get());
				}
				catch (...)
				{
					if (err)
						err(std::current_exception());
				}
			})
			.detach();
	}

   public:
   template <class F>
	[[nodiscard]]decltype(auto) WithSync(F&& f) const
	{
		if (IsCluster)
			return std::forward<F>(f)(*HandleCluster);
		return std::forward<F>(f)(*Handle);
	}

	template <class F>
	[[nodiscard]] decltype(auto) WithAsync(F&& f) const
	{
		if (IsCluster)
			return std::forward<F>(f)(*HandleAsyncCluster);
		return std::forward<F>(f)(*HandleAsync);
	}
	RedisConnection(std::unique_ptr<sw::redis::Redis> redis,
					std::unique_ptr<sw::redis::AsyncRedis> asyncRedis)
		: IsCluster(false),
		  Handle(std::move(redis)),
		  HandleAsync(std::move(asyncRedis))
	{
	}

	RedisConnection(std::unique_ptr<sw::redis::RedisCluster> cluster,
					std::unique_ptr<sw::redis::AsyncRedisCluster> asyncCluster)
		: IsCluster(true),
		  HandleCluster(std::move(cluster)),
		  HandleAsyncCluster(std::move(asyncCluster))
	{
	}

	// -------------------------
	// Example: EXISTS
	// -------------------------
	/**
	 * @brief Count how many of the given key(s) exist.
	 * @details Maps to Redis EXISTS. For a single key, result is 0 or 1.
	 * @note In Cluster, this is a single-key op (safe). Multi-key EXISTS has
	 * slot restrictions.
	 */
	[[nodiscard]] long long ExistsCount(std::string_view key) const;

	/**
	 * @brief Test whether a key exists.
	 * @details Convenience wrapper around ExistsCount(key) != 0.
	 */
	[[nodiscard]] bool Exists(std::string_view key) const;

	/**
	 * @brief Async count how many of the given key(s) exist.
	 * @return std::future resolving to 0 or 1 for a single key.
	 */
	[[nodiscard]] std::future<long long> ExistsCountAsync(
		std::string_view key) const;

	/**
	 * @brief Async test whether a key exists (std::future<bool>).
	 * @details Implemented by mapping std::future<long long> →
	 * std::future<bool>.
	 */
	[[nodiscard]] std::future<bool> ExistsAsync(std::string_view key) const;

	/**
	 * @brief Async test whether a key exists (callback).
	 * @param key Key to check.
	 * @param ok Called with true/false once ready.
	 * @param err Called if the underlying std::future throws.
	 */
	void ExistsAsyncCb(std::string_view key, ResultCb<bool> ok,
					   ErrorCb err = {}) const;

	/**
	 * @brief Delete a key.
	 * @details Maps to Redis DEL. Returns number of keys removed (0 or 1 here).
	 */
	[[nodiscard]] long long Del(std::string_view key) const;

	/** @brief Async DEL (std::future<long long>). */
	[[nodiscard]] std::future<long long> DelAsync(std::string_view key) const;

	/** @brief Async DEL (callback). */
	void DelAsyncCb(std::string_view key, ResultCb<long long> ok,
					ErrorCb err = {}) const;

	/**
	 * @brief Set a key's time to live in seconds.
	 * @details Maps to Redis EXPIRE.
	 * @return true if the timeout was set; false if key does not exist.
	 */
	[[nodiscard]] bool Expire(std::string_view key,
							  std::chrono::seconds ttl) const;

	/** @brief Async EXPIRE (std::future<bool>). */
	[[nodiscard]] std::future<bool> ExpireAsync(std::string_view key,
												std::chrono::seconds ttl) const;

	/** @brief Async EXPIRE (callback). */
	void ExpireAsyncCb(std::string_view key, std::chrono::seconds ttl,
					   ResultCb<bool> ok, ErrorCb err = {}) const;

	/**
	 * @brief Get the remaining time to live of a key (seconds).
	 * @details Maps to Redis TTL. -1 means no expire, -2 means key does not
	 * exist.
	 */
	[[nodiscard]] long long TTL(std::string_view key) const;

	/** @brief Async TTL (std::future<long long>). */
	[[nodiscard]] std::future<long long> TTLAsync(std::string_view key) const;

	/** @brief Async TTL (callback). */
	void TTLAsyncCb(std::string_view key, ResultCb<long long> ok,
					ErrorCb err = {}) const;

	// -------------------------
	// Example: GET / SET
	// -------------------------
	/**
	 * @brief Get the string value of a key.
	 * @details Maps to Redis GET.
	 * @return std::optional<std::string> with a value if the key exists,
	 * otherwise empty.
	 */
	[[nodiscard]] std::optional<std::string> Get(std::string_view key) const;

	/** @brief Async GET (std::future<std::optional<std::string>>). */
	[[nodiscard]] std::future<std::optional<std::string>> GetAsync(
		std::string_view key) const;

	/** @brief Async GET (callback). */
	void GetAsyncCb(std::string_view key,
					ResultCb<std::optional<std::string>> ok,
					ErrorCb err = {}) const;

	/**
	 * @brief Set the string value of a key.
	 * @details Maps to Redis SET.
	 * @return true on OK.
	 */
	[[nodiscard]] bool Set(std::string_view key, std::string_view value) const;

	/** @brief Async SET (std::future<bool>). */
	[[nodiscard]] std::future<bool> SetAsync(std::string_view key,
											 std::string_view value) const;

	/** @brief Async SET (callback). */
	void SetAsyncCb(std::string_view key, std::string_view value,
					ResultCb<bool> ok, ErrorCb err = {}) const;
	/**
	 * @brief Delete the entire hash key (and all its fields) by deleting the
	 * key.
	 * @return Number of keys removed: 0 if key didn't exist, 1 if deleted.
	 */
	[[nodiscard]] long long DelKey(const std::string_view& key) const;
	/**
	 * @brief Async delete of the entire hash key.
	 * @return std::future resolving to 0 or 1.
	 */
	[[nodiscard]] std::future<long long> DelKeyAsync(
		const std::string_view& key) const;
	// -------------------------
	// Callback-style async (optional)
	// -------------------------
	template <typename ReplyT, typename Fn>
	void GetAsyncCb(const std::string_view& key, Fn&& cb) const
	{
		WithAsyncRedisForKey(
			std::string_view{key},
			[&](auto& r)
			{
				r.get(key,
					  std::forward<Fn>(cb));  // cb: void(std::future<ReplyT>&&)
			});
	}

	// =========================================================================
	// Hashes (HSET / HGET / HEXISTS / HDEL / HLEN / HINCRBY / HMGET)
	// =========================================================================

	/**
	 * @brief Set a single field in a hash (HSET).
	 *
	 * Routes by @p key (cluster-safe).
	 *
	 * @param key Hash key.
	 * @param field Field name.
	 * @param value Field value.
	 * @return Number of fields that were newly added (0 if field existed and
	 * was overwritten, 1 if new).
	 */
	[[nodiscard]] long long HSet(const std::string_view& key,
								 const std::string_view& field,
								 const std::string_view& value) const;

	/**
	 * @brief Async HSET (future-based).
	 *
	 * @return std::future resolving to number of newly added fields (0 or 1).
	 */
	[[nodiscard]] std::future<long long> HSetAsync(
		const std::string_view& key, const std::string_view& field,
		const std::string_view& value) const;

	/**
	 * @brief Get a single field from a hash (HGET).
	 *
	 * @return Optional string: empty if the field does not exist.
	 */
	[[nodiscard]] std::optional<std::string> HGet(
		const std::string_view& key, const std::string_view& field) const;

	/**
	 * @brief Async HGET (future-based).
	 *
	 * @return std::future resolving to std::optional<std::string> (empty if
	 * field missing).
	 */
	[[nodiscard]] std::future<std::optional<std::string>> HGetAsync(
		const std::string_view& key, const std::string_view& field) const;
	
	// Returns all fields + values.
	// NOTE: can be big; consider HSCAN for huge hashes.
	[[nodiscard]] std::unordered_map<std::string, std::string> HGetAll(
		const std::string_view& key) const;

	[[nodiscard]] std::future<std::unordered_map<std::string, std::string>>
	HGetAllAsync(const std::string_view& key) const;
	/**
	 * @brief Check if a field exists in a hash (HEXISTS).
	 */
	[[nodiscard]] bool HExists(const std::string_view& key,
							   const std::string_view& field) const;

	/**
	 * @brief Async HEXISTS (future-based).
	 */
	[[nodiscard]] std::future<bool> HExistsAsync(
		const std::string_view& key, const std::string_view& field) const;

	/**
	 * @brief Delete one or more fields from a hash (HDEL).
	 *
	 * @param fields List of fields to delete.
	 * @return Number of fields that were removed.
	 */
	[[nodiscard]] long long HDel(
		const std::string_view& key,
		const std::vector<std::string_view>& fields) const;

	/**
	 * @brief Async HDEL (future-based).
	 *
	 * @return std::future resolving to number of removed fields.
	 */
	[[nodiscard]] std::future<long long> HDelAsync(
		const std::string_view& key,
		const std::vector<std::string_view>& fields) const;

	[[nodiscard]] auto HDelAll(const std::string_view& key) const
	{
		return DelKey(key);
	}
	[[nodiscard]] auto HDelAllAsync(const std::string_view& key) const
	{
		return DelKeyAsync(key);
	}
	/**
	 * @brief Get number of fields in a hash (HLEN).
	 */
	[[nodiscard]] long long HLen(const std::string_view& key) const;

	/**
	 * @brief Async HLEN (future-based).
	 */
	[[nodiscard]] std::future<long long> HLenAsync(
		const std::string_view& key) const;

	/**
	 * @brief Increment an integer field by @p by (HINCRBY).
	 *
	 * Creates the field if it does not exist, treating missing as 0.
	 *
	 * @return The new value after increment.
	 */
	[[nodiscard]] long long HIncrBy(const std::string_view& key,
									const std::string_view& field,
									long long by) const;

	/**
	 * @brief Async HINCRBY (future-based).
	 */
	[[nodiscard]] std::future<long long> HIncrByAsync(
		const std::string_view& key, const std::string_view& field,
		long long by) const;

	/**
	 * @brief Fetch multiple fields from a hash (HMGET).
	 *
	 * @param fields Fields to fetch.
	 * @return Vector of std::optional<std::string> aligned with @p fields
	 * order.
	 */
	[[nodiscard]] std::vector<std::optional<std::string>> HMGet(
		const std::string_view& key,
		const std::vector<std::string_view>& fields) const;

	/**
	 * @brief Async HMGET (future-based).
	 *
	 * @return std::future resolving to vector of std::optional<std::string>
	 * aligned with @p fields.
	 */
	[[nodiscard]] std::future<std::vector<std::optional<std::string>>>
	HMGetAsync(const std::string_view& key,
			   const std::vector<std::string_view>& fields) const;

	/**
	 * @brief Callback-style async HGET.
	 *
	 * Callback runs on the async event loop thread; keep it fast (enqueue work
	 * elsewhere).
	 *
	 * Expected callback signature:
	 *   void(std::future<std::optional<std::string>>&&)
	 */
	template <typename Fn>
	void HGetAsyncCb(const std::string_view& key, const std::string_view& field,
					 Fn&& cb) const
	{
		WithAsyncRedisForKey(std::string_view{key}, [&](auto& r)
							 { r.hget(key, field, std::forward<Fn>(cb)); });
	}
	// =========================================================================
	// Sets (SADD / SREM / SISMEMBER / SCARD / SMEMBERS)
	// =========================================================================

	/**
	 * @brief Add one or more members to a set (SADD).
	 * @return Number of elements actually added (excluding existing ones).
	 */
	[[nodiscard]] long long SAdd(
		const std::string_view& key,
		const std::vector<std::string_view>& members) const;

	/** @brief Async SADD. */
	[[nodiscard]] std::future<long long> SAddAsync(
		const std::string_view& key,
		const std::vector<std::string_view>& members) const;

	/**
	 * @brief Remove one or more members from a set (SREM).
	 * @return Number of elements removed.
	 */
	[[nodiscard]] long long SRem(
		const std::string_view& key,
		const std::vector<std::string_view>& members) const;

	/** @brief Async SREM. */
	[[nodiscard]] std::future<long long> SRemAsync(
		const std::string_view& key,
		const std::vector<std::string_view>& members) const;

	/**
	 * @brief Test if a value is a member of a set (SISMEMBER).
	 */
	[[nodiscard]] bool SIsMember(const std::string_view& key,
								 const std::string_view& member) const;

	/** @brief Async SISMEMBER. */
	[[nodiscard]] std::future<bool> SIsMemberAsync(
		const std::string_view& key, const std::string_view& member) const;

	/**
	 * @brief Get the number of elements in a set (SCARD).
	 */
	[[nodiscard]] long long SCard(const std::string_view& key) const;

	/** @brief Async SCARD. */
	[[nodiscard]] std::future<long long> SCardAsync(
		const std::string_view& key) const;

	/**
	 * @brief Get all members of a set (SMEMBERS).
	 * @note Can be expensive for large sets.
	 */
	[[nodiscard]] std::vector<std::string> SMembers(
		const std::string_view& key) const;

	/** @brief Async SMEMBERS. */
	[[nodiscard]] std::future<std::vector<std::string>> SMembersAsync(
		const std::string_view& key) const;

	// =========================================================================
	// Sorted Sets (ZADD / ZREM / ZSCORE / ZRANGE / ZCARD)
	// =========================================================================

	/**
	 * @brief Add or update a member in a sorted set (ZADD).
	 * @return Number of newly added elements.
	 */
	[[nodiscard]] long long ZAdd(const std::string_view& key,
								 const std::string_view& member,
								 double score) const;

	/** @brief Async ZADD. */
	[[nodiscard]] std::future<long long> ZAddAsync(
		const std::string_view& key, const std::string_view& member,
		double score) const;

	/**
	 * @brief Remove a member from a sorted set (ZREM).
	 * @return Number of elements removed (0 or 1).
	 */
	[[nodiscard]] long long ZRem(const std::string_view& key,
								 const std::string_view& member) const;

	/** @brief Async ZREM. */
	[[nodiscard]] std::future<long long> ZRemAsync(
		const std::string_view& key, const std::string_view& member) const;

	/**
	 * @brief Get the score of a member in a sorted set (ZSCORE).
	 */
	[[nodiscard]] std::optional<double> ZScore(
		const std::string_view& key, const std::string_view& member) const;

	/** @brief Async ZSCORE. */
	[[nodiscard]] std::future<std::optional<double>> ZScoreAsync(
		const std::string_view& key, const std::string_view& member) const;

	/**
	 * @brief Get a range of members by rank (ZRANGE).
	 */
	[[nodiscard]] std::vector<std::string> ZRange(const std::string_view& key,
												  long long start,
												  long long stop) const;

	/** @brief Async ZRANGE. */
	[[nodiscard]] std::future<std::vector<std::string>> ZRangeAsync(
		const std::string_view& key, long long start, long long stop) const;

	/**
	 * @brief Get number of elements in a sorted set (ZCARD).
	 */
	[[nodiscard]] long long ZCard(const std::string_view& key) const;

	/** @brief Async ZCARD. */
	[[nodiscard]] std::future<long long> ZCardAsync(
		const std::string_view& key) const;

	// -------------------------
	// Example: PUB / SUB
	// -------------------------
	/**
	 * @brief Publish a message to a channel.
	 * @details Maps to Redis PUBLISH.
	 * @return number of clients that received the message.
	 */
	[[nodiscard]] int Publish(std::string_view channel,
							  std::string_view message) const
	{
		return WithSync([&](auto& r) -> long long
						{ return r.publish(channel, message); });
	}

	/** @brief Async PUBLISH (std::future<int>). */
	[[nodiscard]] std::future<long long> PublishAsync(
		std::string_view channel, std::string_view message) const
	{
		return WithAsync([&](auto& r) -> std::future<long long>
						 { return r.publish(channel, message); });
	}

	/** @brief Async PUBLISH (callback). */
	void PublishAsyncCb(std::string_view channel, std::string_view message,
						ResultCb<int> ok, ErrorCb err = {}) const;

	/**
	 * @brief Subscribe to a channel.
	 * @details Maps to Redis SUBSCRIBE. The callback is called on every message
	 * received.
	 */
	sw::redis::Subscriber Subscriber() const
	{
		return WithSync([&](auto& r) -> sw::redis::Subscriber
						{ return r.subscriber(); });
	}

	/**
	 * @brief Unsubscribe from a channel.
	 * @details Maps to Redis UNSUBSCRIBE.
	 */
	void Unsubscribe(std::string_view channel) const;

	/**
	 * @brief Subscribe to a pattern (PSUBSCRIBE).
	 * @details The callback is called on every message matching the pattern.
	 */
	void PSubscribe(std::string_view pattern, ResultCb<std::string> onMessage,
					ErrorCb onError = {}) const;

	/**
	 * @brief Unsubscribe from a pattern (PUNSUBSCRIBE).
	 */
	void PUnsubscribe(std::string_view pattern) const;

	struct RedisTime
	{
		int64_t seconds;
		int64_t microseconds;
	};
	/// @brief Get current Redis server time (authoritative).
	[[nodiscard]] RedisTime GetTimeNow() const;

	/// @brief Convenience helper: seconds since epoch as double.
	[[nodiscard]] double GetTimeNowSeconds() const;
};