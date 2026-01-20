
#pragma once
#include <unordered_map>
#include "Packet.hpp"
#include "Packet/CommandPacket.hpp"
class PacketManager
{
    public:
    using Packet_Callback = std::function<void(const IPacket&)>;
    private:

    struct CallbackEntry
{
    std::atomic<bool> alive{true};
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
        PacketManager* owner = nullptr;
        PacketType type{};
        uint64_t id = 0;

        Subscription() = default;
        Subscription(PacketManager* o, PacketType t, uint64_t i)
            : owner(o), type(t), id(i) {}

        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;

        Subscription(Subscription&& other) noexcept
        {
            *this = std::move(other);
        }

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
                owner->Deactivate(type, id);
                owner = nullptr;
            }
        }
    };

    template<typename TPacket>
    [[nodiscard]] Subscription Subscribe(std::function<void(const TPacket&)> cb)
    {
        static_assert(std::is_base_of_v<IPacket, TPacket>);

        const uint64_t id = m_nextId.fetch_add(1, std::memory_order_relaxed);

        auto entry = std::make_unique<CallbackEntry>();
        entry->id = id;
        entry->cb =
            [cb = std::move(cb)](const IPacket& pkt)
            {
                cb(static_cast<const TPacket&>(pkt));
            };

        {
            std::lock_guard lock(m_mutex);
            m_callbacks[TPacket::Type].push_back(std::move(entry));
        }

        return Subscription{ this, TPacket::Type, id };
    }

    void Dispatch(const IPacket& pkt, PacketType type)
    {
        std::vector<CallbackEntry*> snapshot;

        {
            std::lock_guard lock(m_mutex);
            auto it = m_callbacks.find(type);
            if (it == m_callbacks.end())
                return;

            snapshot.reserve(it->second.size());
            for (auto& e : it->second)
                snapshot.push_back(e.get());
        }

        // Hot path: no locks held
        for (auto* e : snapshot)
        {
            if (e->alive.load(std::memory_order_acquire))
                e->cb(pkt);
        }
    }

    void Cleanup()
    {
        std::lock_guard lock(m_mutex);

        for (auto& [_, vec] : m_callbacks)
        {
            std::erase_if(vec,
                [](auto& e)
                {
                    return !e->alive.load(std::memory_order_acquire);
                });
        }
    }

private:
    void Deactivate(PacketType type, uint64_t id)
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
    std::unordered_map<
        PacketType,
        std::vector<std::unique_ptr<CallbackEntry>>
    > m_callbacks;

    std::mutex m_mutex;
    std::atomic<uint64_t> m_nextId{1};
};
