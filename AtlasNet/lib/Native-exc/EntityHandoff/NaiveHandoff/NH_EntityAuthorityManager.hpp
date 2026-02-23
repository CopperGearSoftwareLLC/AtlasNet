#pragma once

// Naive handoff entrypoint.
// Thin wrapper around an internal runtime class.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "Entity/Entity.hpp"
#include "Global/Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"

class Log;

class NH_EntityAuthorityManager : public Singleton<NH_EntityAuthorityManager>
{
  public:
	// Pending incoming handoff state.
	struct PendingIncomingHandoff
	{
		AtlasEntity entity;
		NetworkIdentity sender;
		uint64_t transferTick = 0;
	};

	// Pending outgoing handoff state.
	struct PendingOutgoingHandoff
	{
		AtlasEntityID entityId = 0;
		NetworkIdentity targetIdentity;
		//std::string targetClaimKey;
		uint64_t transferTick = 0;
	};

	NH_EntityAuthorityManager();
	~NH_EntityAuthorityManager();

	// Lifecycle
	void Init(const NetworkIdentity& self, std::shared_ptr<Log> inLogger);
	void Tick();
	void Shutdown();

	// Incoming handoff packet hooks
	void OnIncomingHandoffEntity(const AtlasEntity& entity,
								 const NetworkIdentity& sender);
	void OnIncomingHandoffEntityAtTick(const AtlasEntity& entity,
									   const NetworkIdentity& sender,
									   uint64_t transferTick);

	[[nodiscard]] bool IsInitialized() const;

  private:
	class Runtime;
	std::unique_ptr<Runtime> runtime;
};
