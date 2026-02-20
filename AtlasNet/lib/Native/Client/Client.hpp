#pragma once
#include "Global/Misc/UUID.hpp"
#include "Network/IPAddress.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
using ClientID = UUID;
struct Client
{
	IPAddress ip;
	ClientID ID;

   public:
	static Client MakeNewClient(UUID id, IPAddress ip)
	{
		Client c;
		c.ID = UUIDGen::Gen();
		c.ip = ip;
		return c;
	}

	void Serialize(ByteWriter& bw) const
	{
		ip.Serialize(bw);
		bw.uuid(ID);
	}
	void Deserialize(ByteReader& br)
	{
		ip.Deserialize(br);
		ID = br.uuid();
	}
};