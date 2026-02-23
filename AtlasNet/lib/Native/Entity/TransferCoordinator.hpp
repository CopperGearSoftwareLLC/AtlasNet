#pragma once

#include <algorithm>
#include <boost/container/small_vector.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index_container_fwd.hpp>
#include <chrono>
#include <execution>
#include <mutex>
#include <stop_token>
#include <thread>

#include "Debug/Log.hpp"
#include "Entity/Entity.hpp"
#include "Entity/EntityLedger.hpp"
#include "Entity/Packet/ClientTransferPacket.hpp"
#include "Entity/Packet/EntityTransferPacket.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Global/Misc/UUID.hpp"
class TransferCoordinator : public Singleton<TransferCoordinator>
{
	struct TransferByID;
	using TransferID = UUID;
	struct EntityTransferData
	{
		TransferID ID;
		EntityTransferPacket::TransferStage stage;
		boost::container::small_vector<AtlasEntityID, 32> entityIDs;
		bool WaitingOnResponse = false;
		bool Started = false;
	};
	/* boost::multi_index_container<
		EntityTransferData,
		boost::multi_index::indexed_by<boost::multi_index::hashed_unique<
			boost::multi_index::tag<TransferByID>,
			boost::multi_index::member<EntityTransferData, TransferID, &EntityTransferData::ID>>>>
		EntityTransfers; */
	std::unordered_map<TransferID, EntityTransferData> EntityTransfers;
	struct ClientTransferData
	{
		TransferID ID;
		ClientTransferPacket::MsgStage stage;
		boost::container::small_vector<AtlasEntityID, 32> entityIDs;
		bool WaitingOnResponse = false;
		bool Started = false;
	};
	std::unordered_map<TransferID, ClientTransferData> ClientTransfers;

	std::mutex EntityTransferMutex, ClientTransferMutex;
	std::jthread TransferThread;
	Log logger = Log("TransferCoordinator");

   public:
	TransferCoordinator()
	{
		TransferThread = std::jthread([this](std::stop_token st) { TransferThreadEntry(st); });
	};
	void MarkEntitiesForTransfer(const std::span<AtlasEntityID> entities)
	{
		logger.DebugFormatted("Marked {} entities for transfer", entities.size());
		std::optional<EntityTransferData> ed;
		std::optional<ClientTransferData> cd;
		for (const auto& entityID : entities)
		{
			const bool isClient = EntityLedger::Get().IsEntityClient(entityID);

			if (isClient)
			{
				if (!cd.has_value())
				{
					cd.emplace();
					cd->ID = UUIDGen::Gen();
				}

				cd->entityIDs.push_back(entityID);
			}
			else
			{
				if (!ed.has_value())
				{
					ed.emplace();
					ed->ID = UUIDGen::Gen();
				}
				ed->entityIDs.push_back(entityID);
			}
		}
		if (cd.has_value())
		{
			std::lock_guard<std::mutex> lock(ClientTransferMutex);

			ClientTransfers.insert(std::make_pair(cd->ID, *cd));
		}

		if (ed.has_value())
		{
			std::lock_guard<std::mutex> lock(EntityTransferMutex);
			EntityTransfers.insert(std::make_pair(ed->ID, *ed));
		}
	};

	void TransferThreadEntry(std::stop_token st)
	{
		while (!st.stop_requested())
		{
			{
				std::lock_guard<std::mutex> lock(EntityTransferMutex);
				if (!EntityTransfers.empty())
				{
					// std::for_each(std::execution::seq, EntityTransfers.begin(),
					//			  EntityTransfers.end(),
					//			  [](std::pair<TransferID, EntityTransferData>& transferdata) {
					//
					//			  });
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
	}
};