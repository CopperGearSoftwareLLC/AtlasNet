#pragma once
#include "atlasnet/core/messages/Message.hpp"
#include "steam/steamtypes.h"

ATLASNET_MESSAGE(
    RpcRequestMessage,
    ATLASNET_MESSAGE_DATA(uint32_t, methodId),
    ATLASNET_MESSAGE_DATA(uint32_t, callID),
    ATLASNET_MESSAGE_DATA(std::vector<uint8>, payload)
);
ATLASNET_MESSAGE(
    RpcResponseMessage,
    ATLASNET_MESSAGE_DATA(uint32_t, methodId),
    ATLASNET_MESSAGE_DATA(uint32_t, callID),
    ATLASNET_MESSAGE_DATA(std::vector<uint8>, payload)
);
ATLASNET_MESSAGE(
    RpcErrorMessage,
    ATLASNET_MESSAGE_DATA(uint32_t, methodId),
    ATLASNET_MESSAGE_DATA(uint32_t, callID),
    ATLASNET_MESSAGE_DATA(std::string, ErrorMsg)
);
