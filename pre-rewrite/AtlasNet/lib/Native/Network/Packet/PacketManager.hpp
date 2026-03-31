#pragma once
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

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
        Packet_Callback cb;
    };

    using CallbackPtr = std::shared_ptr<CallbackEntry>;
    using WeakCallbackPtr = std::weak_ptr<CallbackEntry>;

public:
    struct Subscription
    {
    private:
        CallbackPtr handle;

    public:
        Subscription() = default;
        explicit Subscription(CallbackPtr h) : handle(std::move(h)) {}

        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;

        Subscription(Subscription&&) noexcept = default;
        Subscription& operator=(Subscription&&) noexcept = default;

        ~Subscription() { Reset(); }

        void Reset()
        {
            handle.reset(); 
        }

        bool IsValid() const { return handle != nullptr; }
    };

public:
    template <typename TPacket>
    requires std::derived_from<TPacket, IPacket>
    [[nodiscard]] Subscription Subscribe(
        std::function<void(const TPacket&, const PacketInfo&)> cb)
    {
        std::lock_guard lock(m_mutex);

        auto wrapper = std::make_shared<CallbackEntry>();
        wrapper->cb = [cb = std::move(cb)](const IPacket& pkt, const PacketInfo& info)
        {
            cb(static_cast<const TPacket&>(pkt), info);
        };

        m_callbacks[TPacket::TypeID].push_back(wrapper);

        logger.DebugFormatted("New subscription to {}", TPacket::GetPacketNameStatic());

        return Subscription{wrapper};
    }

    void Dispatch(const IPacket& pkt, PacketTypeID type, const PacketInfo& info)
    {
        std::vector<CallbackPtr> alive;

        {
            std::lock_guard lock(m_mutex);

            auto it = m_callbacks.find(type);
            if (it == m_callbacks.end())
                return;

            auto& vec = it->second;

            // Clean + collect alive callbacks
            size_t write = 0;
            for (size_t read = 0; read < vec.size(); ++read)
            {
                if (auto cb = vec[read].lock())
                {
                    alive.push_back(cb);
                    vec[write++] = vec[read];
                }
            }

            vec.resize(write); // remove dead ones
        }

        //logger.DebugFormatted("Dispatching {} callbacks to {}", alive.size(),
        //                      pkt.GetPacketName());

        // No locks here
        for (const auto& cb : alive)
        {
            cb->cb(pkt, info);
        }
    }

    void Cleanup()
    {
        std::lock_guard lock(m_mutex);
        m_callbacks.clear();
    }

private:
    std::unordered_map<PacketTypeID, std::vector<WeakCallbackPtr>> m_callbacks;
    std::mutex m_mutex;
};