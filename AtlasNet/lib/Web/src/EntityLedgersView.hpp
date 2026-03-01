#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <unordered_map>

#include "Client/Client.hpp"
#include "Debug/Log.hpp"
#include "Entity/Entity.hpp"
#include "Entity/Packet/LocalEntityListRequestPacket.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/pch.hpp"
#include "Heuristic/Database/HeuristicManifest.hpp"
#include "Heuristic/IBounds.hpp"
#include "Interlink/Database/ServerRegistry.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/PacketManager.hpp"
struct EntityLedgerEntry
{
	std::string EntityID;
	std::string ClientID;
	bool ISClient;
	float positionx, positiony, positionz;
	int WorldID;
	int BoundID;
};
class EntityLedgersView
{
	Log logger = Log("EntityLedgersView");
	PacketManager::Subscription subToLocalEntityListRequestPacket;
	std::mutex mtx;
	std::condition_variable cv;

	uint32_t RequestsUnanswered = 0;  // no longer atomic
	std::vector<EntityLedgerEntry> EntityListResponses;
	void OnLocalEntityListRequestPacket(const LocalEntityListRequestPacket& p,
										const PacketManager::PacketInfo& info)
	{
		if (p.status != LocalEntityListRequestPacket::MsgStatus::eResponse)
		{
			return;
		}

		std::unique_lock<std::mutex> lock(mtx);

		auto BoundID = HeuristicManifest::Get().BoundIDFromShard(info.sender);

		if (BoundID.has_value())
		{
			for (const auto& ae : std::get<std::vector<AtlasEntityMinimal>>(p.Response_Entities))
			{
				EntityLedgerEntry e;
				e.BoundID = *BoundID;
				e.ClientID = UUIDGen::ToString(ae.Client_ID);
				e.EntityID = UUIDGen::ToString(ae.Entity_ID);
				e.ISClient = ae.IsClient;
				e.positionx = ae.transform.position.x;
				e.positiony = ae.transform.position.y;
				e.positionz = ae.transform.position.z;
				e.WorldID = 0;

				EntityListResponses.push_back(e);
			}
		}
		else
		{
			logger.ErrorFormatted(
				"LocalEntityListRequestPacket arrived but bound ID could not be determined. {}",
				info.sender.ToString());
		}

		if (--RequestsUnanswered == 0)
		{
			cv.notify_one();  // wake waiting thread
		}
	}

   public:
	EntityLedgersView()
	{
		EntityListResponses.clear();
		subToLocalEntityListRequestPacket =
			Interlink::Get().GetPacketManager().Subscribe<LocalEntityListRequestPacket>(
				[this](const auto& p, const auto& info)
				{ OnLocalEntityListRequestPacket(p, info); });
	}
	~EntityLedgersView() {}
	void GetEntityLists(std::vector<EntityLedgerEntry>& entities,
						std::chrono::milliseconds timeout = std::chrono::milliseconds(2000))
	{
		std::unique_lock<std::mutex> lock(mtx);

		EntityListResponses.clear();
		entities.clear();
		RequestsUnanswered = 0;

		auto startTime = std::chrono::high_resolution_clock::now();
		const auto server_list = ServerRegistry::Get().GetServers();

		for (const auto& [netID, Entry] : server_list)
		{
			if (netID.Type == NetworkIdentityType::eShard)
			{
				LocalEntityListRequestPacket p;
				p.status = LocalEntityListRequestPacket::MsgStatus::eQuery;
				p.Request_IncludeMetadata = false;

				Interlink::Get().SendMessage(netID, p, NetworkMessageSendFlag::eUnreliableNow);

				++RequestsUnanswered;
			}
		}

		if (RequestsUnanswered > 0)
		{
			const bool completed =
				cv.wait_for(lock, timeout, [this] { return RequestsUnanswered == 0; });

			if (!completed)
			{
				logger.WarningFormatted("Timeout reached. {} shard(s) did not respond.",
										RequestsUnanswered);
			}
		}

		entities =
			EntityListResponses;  // copy instead of move to avoid race if late packets arrive

		auto endTime = std::chrono::high_resolution_clock::now();
		auto elapsedMs =
			std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

		logger.DebugFormatted("GetEntityLists completed in {}ms. returned {} entries",
							  std::to_string(elapsedMs), entities.size());
	}
};