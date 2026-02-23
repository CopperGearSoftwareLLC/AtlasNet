// SH authority manager facade.
// Public entrypoint used by server runtime to manage entity handoff lifecycle.

#include "SH_ServerAuthorityManager.hpp"

#include <utility>

#include "SH_ServerAuthorityRuntime.hpp"

SH_ServerAuthorityManager::SH_ServerAuthorityManager() = default;
SH_ServerAuthorityManager::~SH_ServerAuthorityManager() = default;

void SH_ServerAuthorityManager::Init(const NetworkIdentity& self,
									 std::shared_ptr<Log> inLogger)
{
	if (!runtime)
	{
		runtime = std::make_unique<SH_ServerAuthorityRuntime>();
	}
	runtime->Init(self, std::move(inLogger));
}

void SH_ServerAuthorityManager::Tick()
{
	if (!runtime)
	{
		return;
	}
	runtime->Tick();
}

void SH_ServerAuthorityManager::Shutdown()
{
	if (!runtime)
	{
		return;
	}
	runtime->Shutdown();
}

void SH_ServerAuthorityManager::OnIncomingHandoffEntity(
	const AtlasEntity& entity, const NetworkIdentity& sender)
{
	if (!runtime)
	{
		return;
	}
	runtime->OnIncomingHandoffEntity(entity, sender);
}

void SH_ServerAuthorityManager::OnIncomingHandoffEntityAtTimeUs(
	const AtlasEntity& entity, const NetworkIdentity& sender,
	const uint64_t transferTimeUs)
{
	if (!runtime)
	{
		return;
	}
	runtime->OnIncomingHandoffEntityAtTimeUs(entity, sender, transferTimeUs);
}

bool SH_ServerAuthorityManager::IsInitialized() const
{
	return runtime && runtime->IsInitialized();
}
