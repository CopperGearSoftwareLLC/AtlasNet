#pragma once

#include "Client/Client.hpp"
#include "Global/Misc/Singleton.hpp"
class ClientCredentials : public Singleton<ClientCredentials>
{
	const ClientID credentials;

   public:
	ClientCredentials(const ClientID& _credentials) : credentials(_credentials) {}
	const ClientID& GetClientID() { return credentials; }
};