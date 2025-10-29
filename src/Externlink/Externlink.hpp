#pragma once
#include "pch.hpp"
#include "Debug/Log.hpp"
#include <future>
#include <string_view>

struct ExternlinkConnection
{
    uint64_t id = 0; // internal short id mapped to HSteamNetConnection
};

struct ExternlinkProperties
{
    uint16_t port = 9000;                ///< Port to listen on / connect to
    bool     startPollingAsync = true;   ///< If true, StartServer/StartClient launches Poll() loop on a worker
    bool     isServer = false;           ///< Choose mode explicitly
    void*    logger = nullptr;           ///< Optional: your logger object (kept as void* to avoid deps)
};

class Externlink
{
public:
    using OnConnectedFn    = std::function<void(const ExternlinkConnection&)>;
    using OnDisconnectedFn = std::function<void(const ExternlinkConnection&)>;
    using OnMessageFn      = std::function<void(const ExternlinkConnection&, std::string_view)>;

public:
    Externlink();
    ~Externlink();

    // Lifecycle
    bool Start(const ExternlinkProperties& props, bool initSocket = true);
    bool StartServer();                                  ///< Creates listen socket on props_.port
    bool StartClient(const std::string& host, uint16_t port); ///< Connects to host:port
    void Stop();                                         ///< Stops polling, closes sockets, kills GNS

    // Polling
    void Poll();                                         ///< Manual tick (if not async)
    std::future<void> StartPollingAsync();               ///< Starts background poller; Stop() joins

    // Messaging
    EResult Send(const ExternlinkConnection& to, const void* data, size_t sz, int sendFlags = k_nSteamNetworkingSend_Reliable);
    EResult SendString(const ExternlinkConnection& to, std::string_view text, int sendFlags = k_nSteamNetworkingSend_Reliable);
    void    Broadcast(const ExternlinkConnection& from, std::string_view text, int sendFlags = k_nSteamNetworkingSend_Reliable);

    // Callbacks
    void SetOnConnected(OnConnectedFn fn);
    void SetOnDisconnected(OnDisconnectedFn fn);
    void SetOnMessage(OnMessageFn fn);

    // Utilities
    bool IsServer() const noexcept { return props_.isServer; }

private:
    // Static -> instance trampoline for GNS callback
    static void SteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);

    // Internal helpers
    void HandleConnecting(SteamNetConnectionStatusChangedCallback_t* info);
    void HandleConnected(SteamNetConnectionStatusChangedCallback_t* info);
    void HandleClosedOrProblem(SteamNetConnectionStatusChangedCallback_t* info);

    // Mapping helpers
    ExternlinkConnection RegisterConnection(HSteamNetConnection hconn);
    void UnregisterConnection(HSteamNetConnection hconn, uint64_t* outId = nullptr);

private:
    // GNS handles
    ISteamNetworkingSockets*   iface_       = nullptr;
    HSteamListenSocket         listen_      = k_HSteamListenSocket_Invalid;
    HSteamNetPollGroup         pollGroup_   = k_HSteamNetPollGroup_Invalid;

    // State
    std::atomic<bool>          running_{false};
    std::future<void>          pollTask_;

    // Config
    ExternlinkProperties       props_{};

    // Connection maps (protected by mutex_)
    std::mutex                 mutex_;
    uint64_t                   nextId_      = 1;
    std::unordered_map<uint64_t, HSteamNetConnection> idToConn_;
    std::unordered_map<HSteamNetConnection, uint64_t> connToId_;

    // Callbacks
    OnConnectedFn              onConnected_;
    OnDisconnectedFn           onDisconnected_;
    OnMessageFn                onMessage_;

    // Single instance for the C-style callback (same assumption as your current Externlink)
    static Externlink*         s_Instance;
};