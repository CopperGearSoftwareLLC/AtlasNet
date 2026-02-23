#pragma once

#include "Global/Misc/Singleton.hpp"
#include "Network/NetworkIdentity.hpp"
class NetworkCredentials : public Singleton<NetworkCredentials>
{
	const NetworkIdentity ID;

   public:
	NetworkCredentials(const NetworkIdentity& ID) : ID(ID) {}

    const NetworkIdentity& GetID() const {return ID;}
};