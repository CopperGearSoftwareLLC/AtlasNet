#include "KDNetServer.hpp"

#include "Interlink/Interlink.hpp"
void KDNetServer::Initialize(KDNetServer::InitializeProperties properties)
{
	logger->Print("KDNet Initialize");
	Interlink::Check();

	Interlink::Get().Initialize(
		InterlinkProperties{.Type = InterlinkType::eGameServer,
							.logger = logger,
							.acceptConnectionFunc = [](const Connection &c) { return true; }});
	Interlink::Get().ConnectToLocalParition();
}

void KDNetServer::Update(std::span<KDEntity> entities, std::vector<KDEntity> &IncomingEntities,
						 std::vector<KDEntityID> &OutgoingEntities)
{
	Interlink::Get().Tick();
}
