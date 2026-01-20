#pragma once
#include <pch.hpp>
#include "Misc/Singleton.hpp"

using SignalType = int32_t;
struct DockerEventsInit
{
    std::function<void(SignalType)> OnShutdownRequest;
};
class DockerEvents : public Singleton<DockerEvents>
{
    static inline DockerEventsInit init_vars;
    public:
    void Init(const DockerEventsInit&);

};