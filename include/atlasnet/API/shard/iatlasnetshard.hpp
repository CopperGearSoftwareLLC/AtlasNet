#pragma once

namespace AtlasNet {
class IShard
{
public:
    virtual ~IShard() = default;

    void AtlasNet_Shard_Init() {
        // Initialization code for the shard
    }

    virtual void OnShutdown() = 0; // Pure virtual function to be implemented by derived classes

};  
}
