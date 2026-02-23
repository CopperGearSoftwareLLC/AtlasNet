#pragma once
#include <steam/steamtypes.h>

#include <boost/container/small_vector.hpp>
#include <cstdint>
#include <span>

#include "Client/Client.hpp"
#include "Global/AtlasObject.hpp"
#include "Global/Misc/UUID.hpp"
#include "Global/Serialize/ByteReader.hpp"
#include "Global/Serialize/ByteWriter.hpp"
#include "Global/pch.hpp"
#include "Transform.hpp"
using AtlasEntityID = UUID;

struct AtlasEntityMinimal : AtlasObject
{
	virtual ~AtlasEntityMinimal() = default;

	
	AtlasEntityID Entity_ID;
	bool IsClient = false;
	ClientID Client_ID;
	uint64_t PacketSeq = 0; //Sequence ID of last update packet
	uint64_t TransferGeneration = 0;
	struct Data
	{
		Transform transform;

		void Serialize(ByteWriter& bw) const { transform.Serialize(bw); }
		void Deserialize(ByteReader& br) { transform.Deserialize(br); }
	} data;

	void Serialize(ByteWriter& bw) const override
	{
		bw.uuid(Entity_ID);
		bw.u8(IsClient);
		bw.uuid(Client_ID);
		bw.u64(PacketSeq);
		bw.u64(TransferGeneration);
		data.Serialize(bw);
	}
	void Deserialize(ByteReader& br) override
	{
		Entity_ID = br.uuid();
		IsClient = br.u8();
		Client_ID = br.uuid();
		PacketSeq = br.u64();
		TransferGeneration = br.u64();
		data.Deserialize(br);
	}
	static AtlasEntityID CreateUniqueID() { return UUIDGen::Gen(); }
};
struct AtlasEntity : AtlasEntityMinimal
{

	boost::container::small_vector<uint8, 32> Metadata;

	void Serialize(ByteWriter& bw) const override
	{
		AtlasEntityMinimal::Serialize(bw);

		bw.blob(Metadata);
	}
	void Deserialize(ByteReader& br) override
	{
		AtlasEntityMinimal::Deserialize(br);
		const auto metadataBlob = br.blob();
		Metadata.assign(metadataBlob.begin(), metadataBlob.end());
	}
};