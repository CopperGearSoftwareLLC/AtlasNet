
#pragma once
#include "atlasnet/API/shard/iatlasnetshard.hpp"

class TankBattleShard : AtlasNet::IShard
{
public:
    TankBattleShard() = default;
    ~TankBattleShard() override = default;

    void Run()
    {
        AtlasNet_Shard_Init();
    }
    void OnShutdown() override {
        // Cleanup code for the shard
    }

};