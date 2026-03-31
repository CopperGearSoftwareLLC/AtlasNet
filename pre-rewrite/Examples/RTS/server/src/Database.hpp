#pragma once

#include <boost/container/flat_map.hpp>
#include <string>

#include "Client/Client.hpp"
#include "Database/Redis/Redis.hpp"
#include "Database/Redis/RedisConnection.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "PlayerColors.hpp"
#include "PlayerData.hpp"
class Database : public Singleton<Database>
{
	std::shared_ptr<RedisConnection> Connection;
	constexpr static const char* playerColorTable = "Players:Colors";

   public:
	Database() { Connection = Redis::Get().ConnectNonCluster("BuiltInDB_Redis", 2380); }

	PlayerTeams PickPlayerTeam(const ClientID& player)
	{
		const std::string playerKey = UUIDGen::ToString(player);

		const auto result = Connection->HGetAll(playerColorTable);
		boost::container::small_flat_map<ClientID, PlayerTeams, AllPlayerColors.size()>
			ClaimedColors;
		for (const auto& [key, value] : result)
		{
			ClaimedColors.emplace(UUIDGen::FromString(key), PlayerTeamFromString(value));
		}

		auto existing = Connection->HGet(playerColorTable, playerKey);
		if (existing.has_value())
		{
			ByteReader br(existing.value());
			return br.read_scalar<PlayerTeams>();
		}

		// 2. Define a pool of colors

		// 3. Deterministic pick using hash
		while (true)
		{
			std::string playerKey = UUIDGen::ToString(player);
			size_t hash = std::hash<std::string>{}(playerKey);
			PlayerTeams candidate = AllPlayerColors[hash % AllPlayerColors.size()];

			bool taken = false;
			for (const auto& [_, color] : ClaimedColors)
			{
				if (color == candidate)
				{
					taken = true;
					break;
				}
			}
			if (!taken)
			{
				// Found an available color
				Connection->HSet(playerColorTable, playerKey, PlayerTeamToString(candidate));
				return candidate;
			}

			// If taken, modify the key slightly and try again
			playerKey += "_";
		}
	};
};