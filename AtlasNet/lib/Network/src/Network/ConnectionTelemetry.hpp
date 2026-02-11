#pragma once
#include <iostream>
#include <string>
#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"

struct ConnectionTelemetry
{
    std::string IdentityId;
    std::string targetId;

    int pingMs;

    float inBytesPerSec;
    float outBytesPerSec;

    float inPacketsPerSec;

    uint32_t pendingReliableBytes;
    uint32_t pendingUnreliableBytes;
    uint32_t sentUnackedReliableBytes;

    uint64_t queueTimeUsec;

    float qualityLocal;
    float qualityRemote;

    int state;

    void Serialize(ByteWriter& bw) const
	{
        bw.str(IdentityId);
        bw.str(targetId);
        bw.i32(pingMs);
        bw.f32(inBytesPerSec);
        bw.f32(outBytesPerSec);
        bw.f32(inPacketsPerSec);
        bw.u32(pendingReliableBytes);
        bw.u32(pendingUnreliableBytes);
        bw.u32(sentUnackedReliableBytes);
        bw.u64(queueTimeUsec);
        bw.f32(qualityLocal);
        bw.f32(qualityRemote);
        bw.i32(state);
	}
	void Deserialize(ByteReader& br)
	{
        IdentityId = br.str();
        targetId = br.str();
        pingMs = br.i32();
        inBytesPerSec = br.f32();
        outBytesPerSec = br.f32();
        inPacketsPerSec = br.f32();
        pendingReliableBytes = br.u32();
        pendingUnreliableBytes = br.u32();
        sentUnackedReliableBytes = br.u32();
        queueTimeUsec = br.u64();
        qualityLocal = br.f32();
        qualityRemote = br.f32();
        state = br.i32();
	}

        void DebugLogs() const
        {
                std::cerr << "ConnectionTelemetry:" << std::endl;
                std::cerr << "  IdentityId: " << IdentityId << std::endl;
                std::cerr << "  targetId: " << targetId << std::endl;
                std::cerr << "  pingMs: " << pingMs << std::endl;
                std::cerr << "  inBytesPerSec: " << inBytesPerSec << std::endl;
                std::cerr << "  outBytesPerSec: " << outBytesPerSec << std::endl;
                std::cerr << "  inPacketsPerSec: " << inPacketsPerSec << std::endl;
                std::cerr << "  pendingReliableBytes: " << pendingReliableBytes << std::endl;
                std::cerr << "  pendingUnreliableBytes: " << pendingUnreliableBytes << std::endl;
                std::cerr << "  sentUnackedReliableBytes: " << sentUnackedReliableBytes << std::endl;
                std::cerr << "  queueTimeUsec: " << queueTimeUsec << std::endl;
                std::cerr << "  qualityLocal: " << qualityLocal << std::endl;
                std::cerr << "  qualityRemote: " << qualityRemote << std::endl;
                std::cerr << "  state: " << state << std::endl;
        }
};