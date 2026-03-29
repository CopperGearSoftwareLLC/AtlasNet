#pragma once


class ISystem
{
public:
    virtual ~ISystem() = default;
    virtual void Run() = 0;
    virtual void Stop() = 0;
};