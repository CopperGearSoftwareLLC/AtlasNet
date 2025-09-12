#pragma once
#include "pch.hpp"

struct InterlinkPacket {
    uint32_t packetID;
    size_t size;
    std::byte data[];
};
std::unique_ptr<InterlinkPacket> MakeInterlinkPacket(size_t PayloadSize) {
    char* data = new char[PayloadSize];
    new (data) InterlinkPacket();
    return std::unique_ptr<InterlinkPacket>(reinterpret_cast<InterlinkPacket*>(data));
}