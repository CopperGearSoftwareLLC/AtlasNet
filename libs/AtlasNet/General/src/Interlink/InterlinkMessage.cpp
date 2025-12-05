#include "InterlinkMessage.hpp"
#include "Interlink.hpp"
InterlinkMessage &InterlinkMessage::SetSendMethod(InterlinkMessageSendFlag _SendMethod)

{
	SendMethod = _SendMethod;
    return *this;
}

InterlinkMessage &InterlinkMessage::SetPacket(std::shared_ptr<IInterlinkPacket> _Packet)
{
    Packet = _Packet;
    return *this;
}

void InterlinkMessage::SendTo(const InterLinkIdentifier &identifier)
{
    Interlink::Get().SendMessage(*this);
}

bool InterlinkMessage::Validate() const
{
	return false;
}
