#pragma once

#include <atomic>
#include <optional>

#include "Network/NetworkIdentity.hpp"
#include "Log.hpp"
#include "Misc/Singleton.hpp"
#include "Network/Connection.hpp"
#include "Interlink.hpp"
class Proxy : public Singleton<Proxy>
{
	std::shared_ptr<Log> logger = std::make_shared<Log>("Proxy");
	std::atomic_bool ShouldShutdown = false;
	NetworkIdentity ID;

   public:
	void Run();
	void Shutdown();

   private:
	void Init();
	void CleanUp();

	bool OnAcceptConnection(const Connection& c);
	void OnConnected(const NetworkIdentity& id);
	void OnDisconnected(const NetworkIdentity& id);
	void OnMessageReceived(const Connection& from, std::span<const std::byte> data);
};