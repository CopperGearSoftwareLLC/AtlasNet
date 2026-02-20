#pragma once

#include <cstdint>
#include <string>

#include "Entity/Entity.hpp"
#include "Network/NetworkIdentity.hpp"

// Incoming handoff waiting for its agreed transfer time (Unix microseconds).
struct SH_PendingIncomingHandoff
{
	AtlasEntity entity;
	NetworkIdentity sender;
	uint64_t transferTimeUs = 0;
};

// Outgoing handoff waiting for commit at agreed transfer time (Unix microseconds).
struct SH_PendingOutgoingHandoff
{
	AtlasEntityID entityId = 0;
	NetworkIdentity targetIdentity;
	uint64_t transferTimeUs = 0;
};
