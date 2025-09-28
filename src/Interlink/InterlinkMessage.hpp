#pragma once
#include "InterlinkPacket.hpp"
#include <pch.hpp>

enum class InterlinkMessageSendFlag
{
	eImmidiateOrDrop = k_nSteamNetworkingSend_NoDelay, /// Send it NOW or drop it. extremely
													   /// unreliable but quickest. no batching

	eUnreliableNow =
		k_nSteamNetworkingSend_UnreliableNoNagle, /// Send it unreliable as soon as possible.use
												  /// only if you know that this message will be the
												  /// last for a while

	eUnreliableBatched = k_nSteamNetworkingSend_Unreliable, /// Send it unreliably but batch to
															/// other outgoing messages

	eReliableNow =
		k_nSteamNetworkingSend_ReliableNoNagle, /// Send it reliable as soon as possible.use only if
												/// you know that this message will be the last for
												/// a while

	eReliableBatched =
		k_nSteamNetworkingSend_Reliable, /// Send it reliably but batch to other outgoing messages
};
class InterlinkMessage
{
	InterlinkMessageSendFlag SendMethod = InterlinkMessageSendFlag::eReliableNow;
	std::shared_ptr<IInterlinkPacket> Packet;
    public:
	InterlinkMessage()
	{
	}

	InterlinkMessage &SetSendMethod(InterlinkMessageSendFlag _SendMethod);
    InterlinkMessage& SetPacket(std::shared_ptr<IInterlinkPacket> _Packet);
    bool Validate() const;
};