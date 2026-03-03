#pragma once
#include <chrono>
#include <thread>
#include <utility>

#include "Network/NetworkCredentials.hpp"
#include "Network/NetworkIdentity.hpp"
#include "InternalDB/InternalDB.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "Debug/Log.hpp"
class HealthManifest : public Singleton<HealthManifest>
{
	const std::string HealthPingTable = "Health_Ping";

	std::optional<NetworkIdentity> identifier;
	std::jthread HealthPingIntervalFunc;

	std::jthread HealthCheckOnFailureFunc;

   public:
	void ScheduleHealthPings()
	{
		Log logger("HealthManifest");
		logger.DebugFormatted("ScheduleHealthPings: interval_ms={}", _HEALTH_PING_INTERVAL_MS);
		HealthPingIntervalFunc = std::jthread(
			[logger](std::stop_token st) mutable
			{
				logger.Debug("Health ping thread started");
				while (!st.stop_requested())
				{
					const auto id = NetworkCredentials::Get().GetID();
					logger.DebugFormatted("Health ping for {}", id.ToString());
					HealthManifest::Get().HealthUpdate(NetworkCredentials::Get().GetID());
					std::this_thread::sleep_for(
						std::chrono::milliseconds(_HEALTH_PING_INTERVAL_MS));
				}
				logger.Debug("Health ping thread stopping");
			});
	}

	void ScheduleHealthChecks(std::function<void(const NetworkIdentity&,const std::string&)> onHealthCheckFail)
	{
		Log logger("HealthManifest");
		logger.DebugFormatted("ScheduleHealthChecks: interval_ms={}", _HEALTH_CHECK_INTERVAL_MS);
		HealthCheckOnFailureFunc = std::jthread(
			[onHealthCheckFail = std::move(onHealthCheckFail), logger](std::stop_token st) mutable
			{
				logger.Debug("Health check thread started");
				while (!st.stop_requested())
				{
					static std::vector<std::string> expired_pings;
					HealthManifest::Get().GetExpiredPings(expired_pings);
					if (!expired_pings.empty())
					{
						logger.DebugFormatted("Health check: {} expired ping(s) detected",
											   expired_pings.size());
						for (const auto expired_key : expired_pings)
						{
							ByteReader br(expired_key);
							NetworkIdentity id;
							id.Deserialize(br);
							//ASSERT(
							//	expired_ID.has_value(),
							//	std::format("Failed to parse key in expired pings: {}", expired_key)
							//		.c_str());
							logger.WarningFormatted("Health check fail for {} (key={})",
													id.ToString(), expired_key);
							onHealthCheckFail(id, expired_key);
						}
					}
					std::this_thread::sleep_for(
						std::chrono::milliseconds(_HEALTH_CHECK_INTERVAL_MS));
				}
				logger.Debug("Health check thread stopping");
			});
	}
	//=================================
	//===          CHECK            ===
	//=================================
	template <typename KeyType = std::string>
	void GetAllPings(std::unordered_map<KeyType, double>& out_pings);

	template <typename KeyType = std::string>
	void GetExpiredPings(std::vector<KeyType>& out_expired);

	template <typename KeyType = std::string>
	void GetLivePings(std::vector<KeyType>& out_live); // this one

	template <typename Keytype = std::string>
	void RemovePing(const Keytype& key);
   private:
	//=================================
	//===          PING             ===
	//=================================

	void HealthUpdate(const NetworkIdentity& identifier)
	{
		ByteWriter bw;
		identifier.Serialize(bw);

		const double now = InternalDB::Get()->GetTimeNowSeconds();

		const double TTL = now + _HEALTH_PING_TIMESTAMP_LIFE_MS*0.001;

		const auto setResult = InternalDB::Get()->HSet(HealthPingTable, bw.as_string_view(), std::to_string(TTL));
		if (setResult != 0)
		{
			std::printf("Failed to update health in Health Manifest. HSET result: %lli",setResult);
		}
	
	}
};
template <typename KeyType>
inline void HealthManifest::RemovePing(const KeyType& key)
{
	InternalDB::Get()->HDel(HealthPingTable, {key});
}
template <typename KeyType>
inline void HealthManifest::GetAllPings(std::unordered_map<KeyType, double>& out_pings)
{
	out_pings.clear();

	// Get all members in the lease ZSET
	const auto All_Pings = InternalDB::Get()->HGetAll(HealthPingTable);

	out_pings.reserve(All_Pings.size());

	for (const auto& pings : All_Pings)
	{
		if constexpr (std::is_same_v<KeyType, std::string>)
		{
			out_pings.emplace(pings.first, std::stod(pings.second));
		}
		else
		{
			out_pings.emplace(std::move(pings.first), std::stod(pings.second));
		}
	}
}

template <typename KeyType>
inline void HealthManifest::GetExpiredPings(std::vector<KeyType>& out_expired)
{
	out_expired.clear();
	const auto now = InternalDB::Get()->GetTimeNowSeconds();
	std::unordered_map<KeyType, double> all_pings;
	GetAllPings(all_pings);

	for (const auto& [member, scoreOpt] : all_pings)
	{
		if (scoreOpt > now)
		{
			continue;
		}

		if constexpr (std::is_same_v<KeyType, std::string>)
		{
			out_expired.emplace_back(member);
		}
		else
		{
			KeyType key{};
			ByteReader br(member);
			key.Deserialize(br);
			out_expired.emplace_back(std::move(key));
		}
	}
}
template <typename KeyType>
inline void HealthManifest::GetLivePings(std::vector<KeyType>& out_live)
{
	out_live.clear();
	const auto now = InternalDB::Get()->GetTimeNowSeconds();
	std::unordered_map<KeyType, double> all_pings;
	GetAllPings(all_pings);

	for (const auto& [member, scoreOpt] : all_pings)
	{
		if (scoreOpt > now)
		{
			continue;
		}

		if constexpr (std::is_same_v<KeyType, std::string>)
		{
			out_live.emplace_back(member);
		}
		else
		{
			KeyType key{};
			ByteReader br(member);
			key.Deserialize(br);
			out_live.emplace_back(std::move(key));
		}
	}
}