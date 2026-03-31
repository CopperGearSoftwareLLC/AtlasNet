#include "EntityHandle.hpp"

#include "Entity/EntityLedger.hpp"
#include "Entity/GlobalEntityLedger.hpp"
#include "Entity/Packet/EntityHandleFetchRequestPacket.hpp"
#include "Global/pch.hpp"
#include "Interlink/Interlink.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
std::shared_future<AtlasEntity> AtlasEntityHandle::GetAsync() const
{
	
	return future;
}
const AtlasEntity& AtlasEntityHandle::Get() const
{
	auto f = GetAsync();
	f.wait();

	EntityData = f.get();  // store locally
	return *EntityData;
}
bool AtlasEntityHandle::IsLocal() const
{
	return EntityLedger::Get().ExistsEntity(id);
}
AtlasEntityHandle::AtlasEntityHandle(AtlasEntityID id) : id(id)
{
	
}
