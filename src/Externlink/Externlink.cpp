#include "Externlink.hpp"

#include <chrono>
#include <iostream>

Externlink* Externlink::s_Instance = nullptr;

Externlink::Externlink() {}
Externlink::~Externlink() { Stop(); }

bool Externlink::Start(const ExternlinkProperties& props, bool initSocket)
{
    std::cerr << "[Externlink] Starting...\n" << std::endl;

    props_     = props;
    s_Instance = this;

    SteamDatagramErrMsg errMsg;

    if (initSocket)
    {
      if (!GameNetworkingSockets_Init(nullptr, errMsg))
      {
        // Optional: if you have a logger type, cast props_.logger and log properly
        std::cerr << "[Externlink] GNS init failed: " << errMsg << "\n";
        return false;
      }
    }

    SteamNetworkingUtils()->SetGlobalCallback_SteamNetConnectionStatusChanged(SteamNetConnectionStatusChanged);
    iface_ = SteamNetworkingSockets();

    std::cout << "[Externlink] Initialized. Mode=" << (props_.isServer ? "Server" : "Client")
              << " port=" << props_.port << "\n";
    return true;
}

bool Externlink::StartServer()
{
    if (!iface_) return false;

    // Listen on 0.0.0.0:port (addr.Clear() -> 0.0.0.0)
    SteamNetworkingIPAddr addr;
    addr.Clear();
    addr.m_port = props_.port;

    listen_    = iface_->CreateListenSocketIP(addr, 0, nullptr);
    pollGroup_ = iface_->CreatePollGroup();
    if (listen_ == k_HSteamListenSocket_Invalid || pollGroup_ == k_HSteamNetPollGroup_Invalid)
    {
        std::cerr << "[Externlink] Failed to create listen socket or poll group\n";
        return false;
    }

    std::cout << "[Externlink] Listening on UDP " << props_.port << "\n";

    if (props_.startPollingAsync)
        StartPollingAsync();

    return true;
}

bool Externlink::StartClient(const std::string& host, uint16_t port)
{
    if (!iface_) return false;

    SteamNetworkingIPAddr addr;
    addr.Clear();
    addr.ParseString(host.c_str());
    addr.m_port = port;

    HSteamNetConnection conn = iface_->ConnectByIPAddress(addr, 0, nullptr);
    if (conn == k_HSteamNetConnection_Invalid)
    {
        std::cerr << "[Externlink] ConnectByIPAddress failed to " << host << ":" << port << "\n";
        return false;
    }

    std::cout << "[Externlink] Connecting to " << host << ":" << port << " ...\n";

    if (props_.startPollingAsync)
        StartPollingAsync();

    return true;
}

void Externlink::Stop()
{
    running_ = false;
    if (pollTask_.valid())
        pollTask_.wait();

    if (iface_)
    {
        if (listen_ != k_HSteamListenSocket_Invalid)
            iface_->CloseListenSocket(listen_);
        if (pollGroup_ != k_HSteamNetPollGroup_Invalid)
            iface_->DestroyPollGroup(pollGroup_);

        // Close any remaining connections
        {
            std::lock_guard lk(mutex_);
            for (auto& kv : idToConn_)
                iface_->CloseConnection(kv.second, 0, nullptr, false);
            idToConn_.clear();
            connToId_.clear();
        }
    }

    GameNetworkingSockets_Kill();
    iface_    = nullptr;
    listen_   = k_HSteamListenSocket_Invalid;
    pollGroup_= k_HSteamNetPollGroup_Invalid;

    std::cout << "[Externlink] Shutdown complete.\n";
}

std::future<void> Externlink::StartPollingAsync()
{
    running_ = true;
    pollTask_ = std::async(std::launch::async, [this]
    {
        while (running_)
        {
            Poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    return std::move(pollTask_);
}

void Externlink::Poll()
{
    if (!iface_) return;

    // Deliver state changes
    iface_->RunCallbacks();

    // Deliver messages
    if (pollGroup_ != k_HSteamNetPollGroup_Invalid)
    {
        ISteamNetworkingMessage* msgs[32];
        int num = iface_->ReceiveMessagesOnPollGroup(pollGroup_, msgs, 32);
        for (int i = 0; i < num; ++i)
        {
            ISteamNetworkingMessage* msg = msgs[i];

            uint64_t id = 0;
            {
                std::lock_guard lk(mutex_);
                auto it = connToId_.find(msg->m_conn);
                if (it != connToId_.end())
                    id = it->second;
            }

            if (id != 0 && onMessage_)
            {
                onMessage_(ExternlinkConnection{id},
                           std::string_view(static_cast<const char*>(msg->m_pData),
                                            static_cast<size_t>(msg->m_cbSize)));
            }

            msg->Release();
        }
    }
}

/* =========================
   Connection status handling
   ========================= */

void Externlink::SteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info)
{
    if (!s_Instance || !s_Instance->iface_) return;

    switch (info->m_info.m_eState)
    {
        case k_ESteamNetworkingConnectionState_Connecting:
            s_Instance->HandleConnecting(info);
            break;

        case k_ESteamNetworkingConnectionState_Connected:
            s_Instance->HandleConnected(info);
            break;

        case k_ESteamNetworkingConnectionState_ClosedByPeer:
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
            s_Instance->HandleClosedOrProblem(info);
            break;

        default:
            // Ignore other transient states
            break;
    }
}

void Externlink::HandleConnecting(SteamNetConnectionStatusChangedCallback_t* info)
{
    // SERVER: Accept incoming and attach to poll group
    if (IsServer() && listen_ != k_HSteamListenSocket_Invalid)
    {
        iface_->AcceptConnection(info->m_hConn);
        if (pollGroup_ != k_HSteamNetPollGroup_Invalid)
            iface_->SetConnectionPollGroup(info->m_hConn, pollGroup_);

        // Note: We add an ID in Connected (not here), to keep symmetry with clients.
        std::cout << "[Externlink] Incoming connection request (server side)\n";
    }
    else
    {
        // CLIENT: Outgoing connect in progress — do nothing here, wait for Connected
        std::cout << "[Externlink] Outgoing connection in progress (client)\n";
    }
}

void Externlink::HandleConnected(SteamNetConnectionStatusChangedCallback_t* info)
{
    // Both server and client land here when the connection is fully established
    if (pollGroup_ != k_HSteamNetPollGroup_Invalid)
        iface_->SetConnectionPollGroup(info->m_hConn, pollGroup_);

    ExternlinkConnection wrapped = RegisterConnection(info->m_hConn);

    std::cout << "[Externlink] Connection established (id=" << wrapped.id << ")\n";
    if (onConnected_) onConnected_(wrapped);
}

void Externlink::HandleClosedOrProblem(SteamNetConnectionStatusChangedCallback_t* info)
{
    uint64_t id = 0;
    UnregisterConnection(info->m_hConn, &id);

    iface_->CloseConnection(info->m_hConn, 0, nullptr, false);
    std::cout << "[Externlink] Connection closed (id=" << id << ")\n";
    if (onDisconnected_ && id != 0) onDisconnected_(ExternlinkConnection{id});
}

/* =========================
   Mapping helpers
   ========================= */

ExternlinkConnection Externlink::RegisterConnection(HSteamNetConnection hconn)
{
    std::lock_guard lk(mutex_);
    const uint64_t id = nextId_++;
    idToConn_[id] = hconn;
    connToId_[hconn] = id;
    return ExternlinkConnection{id};
}

void Externlink::UnregisterConnection(HSteamNetConnection hconn, uint64_t* outId)
{
    std::lock_guard lk(mutex_);
    auto it = connToId_.find(hconn);
    if (it != connToId_.end())
    {
        uint64_t id = it->second;
        connToId_.erase(it);
        idToConn_.erase(id);
        if (outId) *outId = id;
    }
}

/* =========================
   Messaging
   ========================= */

EResult Externlink::Send(const ExternlinkConnection& to, const void* data, size_t sz, int sendFlags)
{
    HSteamNetConnection h = k_HSteamNetConnection_Invalid;
    {
        std::lock_guard lk(mutex_);
        auto it = idToConn_.find(to.id);
        if (it == idToConn_.end())
            return k_EResultFail;
        h = it->second;
    }
    return iface_->SendMessageToConnection(h, data, (uint32_t)sz, sendFlags, nullptr);
}

EResult Externlink::SendString(const ExternlinkConnection& to, std::string_view text, int sendFlags)
{
    return Send(to, text.data(), text.size(), sendFlags);
}

void Externlink::Broadcast(const ExternlinkConnection& from, std::string_view text, int sendFlags)
{
    std::lock_guard lk(mutex_);
    for (const auto& kv : idToConn_)
    {
        if (kv.first == from.id) continue;
        iface_->SendMessageToConnection(kv.second, text.data(), (uint32)text.size(), sendFlags, nullptr);
    }
}

/* =========================
   Callbacks setters
   ========================= */

void Externlink::SetOnConnected(OnConnectedFn fn)    { onConnected_ = std::move(fn); }
void Externlink::SetOnDisconnected(OnDisconnectedFn fn){ onDisconnected_ = std::move(fn); }
void Externlink::SetOnMessage(OnMessageFn fn)        { onMessage_ = std::move(fn); }
