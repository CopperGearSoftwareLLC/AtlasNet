#include "AtlasNetServer.hpp"

#include "Interlink/Interlink.hpp"
void AtlasNetServer::Initialize(AtlasNetServer::InitializeProperties properties)
{
	logger->Print("AtlasNet Initialize");
	Interlink::Check();

	Interlink::Get().Init(
		InterlinkProperties{.ThisID = InterLinkIdentifier(InterlinkType::eGameServer,-1),
							.logger = logger,
							.acceptConnectionFunc = [](const Connection &c) { return true; }});
	Interlink::Get().ConnectToLocalParition();
}

void AtlasNetServer::Update(std::span<AtlasEntity> entities, std::vector<AtlasEntity> &IncomingEntities,
						 std::vector<AtlasEntityID> &OutgoingEntities)
{
	Interlink::Get().Tick();
}
