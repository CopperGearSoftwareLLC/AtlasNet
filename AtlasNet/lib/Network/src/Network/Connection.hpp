#pragma once

#include <iostream>
#include "NetworkEnums.hpp"
#include "Network/IPAddress.hpp"
#include "Network/NetworkEnums.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Network/Packet/Packet.hpp"
#include "pch.hpp"
struct ConnectionProperties
{
	IPAddress address;
};
struct Connection
{
	IPAddress address;
	ConnectionState oldState, state;
	// InterlinkType TargetType = InterlinkType::eInvalid;
	NetworkIdentity target;
	HSteamNetConnection SteamConnection;

	ConnectionKind kind = ConnectionKind::eInternal;
	[[nodiscard]] bool IsInternal() const noexcept
	{
		return target.IsInternal();
	}

	void SetNewState(ConnectionState newState);
	constexpr NetworkIdentityType GetIdentityType() const
	{
		return target.Type;
	}
};
