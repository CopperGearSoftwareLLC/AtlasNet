#pragma once

#include "Entity/Entity.hpp"
#include "Entity/EntityEnums.hpp"
#include "Global/Misc/UUID.hpp"
#include "Network/NetworkIdentity.hpp"
using TransferID = UUID;
enum class TransferMode
{
	eReceiving,eSending
};
struct EntityTransferData
{
	TransferID ID;
	NetworkIdentity shard;
	TransferMode transferMode;
	EntityTransferStage stage = EntityTransferStage::eNone;
	boost::container::small_vector<AtlasEntityID, 32> entityIDs;
	//bool WaitingOnResponse = false;
};
