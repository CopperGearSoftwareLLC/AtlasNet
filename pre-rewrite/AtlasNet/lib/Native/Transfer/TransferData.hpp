#pragma once

#include <boost/describe/enum.hpp>

#include "Client/Client.hpp"
#include "Entity/Entity.hpp"
#include "Global/Misc/UUID.hpp"
#include "Network/NetworkIdentity.hpp"
using TransferID = UUID;
enum class TransferMode
{
	eReceiving,
	eSending,
	eProxy,
};

enum class EntityTransferStage
{
	eNone,		// Nothing done yet
	ePrepare,	// A -> B notify to prepare to receive certain entities
	eReady,		// B -> A acknowledged
	eCommit,	// A -> B //Freeze simulation and remote calls, sends last snapshot to B
	eComplete,	// B -> A acknowledged, transfer complete

};
BOOST_DESCRIBE_ENUM(EntityTransferStage, eNone, ePrepare, eReady, eCommit, eComplete);

struct EntityTransferData
{
	TransferID ID;
	NetworkIdentity shard;
	TransferMode transferMode;
	EntityTransferStage stage = EntityTransferStage::eNone;
	boost::container::small_vector<AtlasEntityID, 32> entityIDs;
	// bool WaitingOnResponse = false;
};

enum class ClientTransferStage
{
	eNone,			 // Nothing done yet

	ePrepare,		 // A -> B notify to prepare to receive certain clients
	eReady,			 // B -> A acknowledged
	eDrained,		 // A -> B, Transmit the serialized client entities.

};
BOOST_DESCRIBE_ENUM(ClientTransferStage, eNone, ePrepare, eReady, eDrained);

enum class ClientSwitchStage
{
	eNone,			 // Nothing done yet

	eRequestSwitch,	 // A -> Proxy, request to switch routing from A to B for client.
	eFreeze,		 // Proxy -> A Acknowledged, Freezing incoming commands for client.
	eActivate,		 // B -> Proxy Start routing commands for client to me.

};
BOOST_DESCRIBE_ENUM(ClientSwitchStage, eNone, eRequestSwitch, eFreeze,
					eActivate);

struct ClientTransferData
{
	TransferID ID;
	NetworkIdentity shard;
	TransferMode transferMode;
	ClientTransferStage stage = ClientTransferStage::eNone;
	boost::container::small_vector<std::pair<ClientID, AtlasEntityID>, 8> Clients;
};