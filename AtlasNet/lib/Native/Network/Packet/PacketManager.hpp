
#pragma once
#include <atomic>
#include <boost/container/flat_map.hpp>
#include <unordered_map>

#include "Debug/Log.hpp"
#include "Network/NetworkIdentity.hpp"
#include "Packet.hpp"
class PacketManager
{
	Log logger = Log("PacketManager");

   public:
	struct PacketInfo
	{
		NetworkIdentity sender;
	};
	using Packet_Callback = std::function<void(const IPacket&, const PacketInfo&)>;

   private:
	struct CallbackEntry
	{
		std::atomic_bool alive{true};
		uint64_t id;
		Packet_Callback cb;
	};

   public:
	PacketManager() = default;
	PacketManager(PacketManager&&) = delete;
	PacketManager& operator=(const PacketManager&) = delete;
	PacketManager& operator=(PacketManager&&) = delete;
	PacketManager(const PacketManager& o) = delete;

	struct Subscription
	{
	   private:
		Log logger = Log("Subscription");

	   public:
		PacketManager* owner = nullptr;
		PacketTypeID type{};
		uint64_t id = 0;

		Subscription() = default;
		Subscription(PacketManager* o, PacketTypeID t, uint64_t i) : owner(o), type(t), id(i) {}

		Subscription(const Subscription&) = delete;
		Subscription& operator=(const Subscription&) = delete;

		Subscription(Subscription&& other) noexcept { *this = std::move(other); }

		Subscription& operator=(Subscription&& other) noexcept
		{
			if (this != &other)
			{
				Reset();
				owner = other.owner;
				type = other.type;
				id = other.id;
				other.owner = nullptr;
			}
			return *this;
		}

		~Subscription() { Reset(); }

		void Reset()
		{
			if (owner)
			{
				logger.DebugFormatted("Deactivated subscription ID {}", id);
				owner->Deactivate(type, id);
				owner = nullptr;
			}
		}
	};

	template <typename TPacket>
	[[nodiscard]] Subscription Subscribe(std::function<void(const TPacket&, const PacketInfo&)> cb)
	{
		static_assert(std::is_base_of_v<IPacket, TPacket>);

		const uint64_t id = m_nextId.fetch_add(1, std::memory_order_relaxed);

		auto entry = std::make_unique<CallbackEntry>();
		entry->id = id;
		entry->cb = [cb = std::move(cb)](const IPacket& pkt, const PacketManager::PacketInfo& info)
		{ cb(static_cast<const TPacket&>(pkt), info); };

		{
			std::lock_guard lock(m_mutex);
			m_callbacks[TPacket::TypeID].push_back(std::move(entry));
		}
		logger.DebugFormatted("New subscription to {}, ID {}", TPacket::GetPacketNameStatic(), id);

		return Subscription{this, TPacket::TypeID, id};
	}

	void Dispatch(const IPacket& pkt, PacketTypeID type, const PacketInfo& info)
	{
		std::vector<CallbackEntry*> snapshot;

		{
			std::lock_guard lock(m_mutex);
			auto it = m_callbacks.find(type);
			if (it == m_callbacks.end())
			{
				// logger.DebugFormatted("Dispatching 0 callbacks to {}",
				//			  pkt.GetPacketName());
				return;
			}

			snapshot.reserve(it->second.size());
			for (auto& e : it->second) snapshot.push_back(e.get());
		}
		// logger.DebugFormatted("Dispatching {} callbacks to {}", snapshot.size(),
		//					  pkt.GetPacketName());
		//  Hot path: no locks held
		for (auto* e : snapshot)
		{
			if (e->alive.load(std::memory_order_acquire))
				e->cb(pkt, info);
		}
	}

	void Cleanup()
	{
		std::lock_guard lock(m_mutex);

		for (auto& [_, vec] : m_callbacks)
		{
			std::erase_if(vec, [](auto& e) { return !e->alive.load(std::memory_order_acquire); });
		}
	}

   private:
	void Deactivate(PacketTypeID type, uint64_t id)
	{
		std::lock_guard lock(m_mutex);

		auto it = m_callbacks.find(type);
		if (it == m_callbacks.end())
			return;

		for (auto& e : it->second)
		{
			if (e->id == id)
			{
				e->alive.store(false, std::memory_order_release);
				break;
			}
		}
	}

   private:
	boost::container::flat_map<PacketTypeID, std::vector<std::unique_ptr<CallbackEntry>>>
		m_callbacks;

	std::mutex m_mutex;
	std::atomic<uint64_t> m_nextId{1};
};
