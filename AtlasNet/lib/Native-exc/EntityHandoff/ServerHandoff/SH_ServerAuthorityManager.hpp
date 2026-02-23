#pragma once

#include <memory>

#include "Entity/Entity.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"

class Log;
class SH_ServerAuthorityRuntime;

// Public entrypoint for ServerHandoff.
// Forwards work to SH_ServerAuthorityRuntime.
class SH_ServerAuthorityManager : public Singleton<SH_ServerAuthorityManager>
{
  public:
	SH_ServerAuthorityManager();
	~SH_ServerAuthorityManager();

	// Lifecycle
	void Init(const NetworkIdentity& self, std::shared_ptr<Log> inLogger);
	void Tick();
	void Shutdown();

	// Network handoff ingress
	void OnIncomingHandoffEntity(const AtlasEntity& entity,
								 const NetworkIdentity& sender);
	void OnIncomingHandoffEntityAtTimeUs(const AtlasEntity& entity,
										 const NetworkIdentity& sender,
										 uint64_t transferTimeUs);

	[[nodiscard]] bool IsInitialized() const;

  private:
	std::unique_ptr<SH_ServerAuthorityRuntime> runtime;
};
