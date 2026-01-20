#pragma once

#include <atomic>

#include "InterlinkIdentifier.hpp"
#include "Log.hpp"
#include "Misc/Singleton.hpp"
#include "Connection.hpp"
class Proxy : public Singleton<Proxy>
{
	std::shared_ptr<Log> logger = std::make_shared<Log>("Proxy");
	std::atomic_bool ShouldShutdown = false;
	InterLinkIdentifier ID;

   public:
	void Run();
	void Shutdown();

   private:
	void Init();
	void CleanUp();

	bool OnAcceptConnection(const Connection& c);
	void OnConnected(const InterLinkIdentifier& id);
	void OnDisconnected(const InterLinkIdentifier& id);
	void OnMessageReceived(const Connection& from, std::span<const std::byte> data);
};