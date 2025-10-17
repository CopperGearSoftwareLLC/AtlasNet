#pragma once
#include <pch.hpp>
#include "Singleton.hpp"

using SignalType = int32;
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