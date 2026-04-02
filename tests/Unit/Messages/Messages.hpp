#pragma once
#include "atlasnet/core/messages/Message.hpp"


ATLASNET_MESSAGE(SimpleMessage, ATLASNET_MESSAGE_DATA(int, aNumber),
                 ATLASNET_MESSAGE_DATA(std::string, str));