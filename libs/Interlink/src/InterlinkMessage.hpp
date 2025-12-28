#pragma once
#include "InterlinkPacket.hpp"
#include <pch.hpp>
#include "InterlinkEnums.hpp"

class InterlinkMessage
{
	InterlinkMessageSendFlag SendMethod = InterlinkMessageSendFlag::eReliableBatched;
	std::shared_ptr<IInterlinkPacket> Packet;
    public:
	InterlinkMessage()
	{
	}

	InterlinkMessage &SetSendMethod(InterlinkMessageSendFlag _SendMethod);
    InterlinkMessage& SetPacket(std::shared_ptr<IInterlinkPacket> _Packet);
	void SendTo(const InterLinkIdentifier& identifier);
    bool Validate() const;
};