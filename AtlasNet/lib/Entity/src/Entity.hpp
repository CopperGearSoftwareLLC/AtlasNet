#pragma once
#include <cstdint>
#include <span>
#include "AtlasObject.hpp"
#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"
#include "Transform.hpp"
#include "pch.hpp"

struct AtlasEntityMinimal: AtlasObject
{
using EntityID = uint64_t;
	using ClientID = uint64_t;
	EntityID Entity_ID;
	Transform transform;
	bool IsClient;
	ClientID Client_ID;
	void Serialize(ByteWriter& bw) const override
	{
		bw.write_scalar(Entity_ID);
		transform.Serialize(bw);
		bw.u8(IsClient);
		bw.write_scalar(Client_ID);

	}
	void Deserialize(ByteReader& br) override
	{
		Entity_ID = br.read_scalar<decltype(Entity_ID)>();
		transform.Deserialize(br);
		IsClient = br.u8();
		Client_ID = br.read_scalar<decltype(Client_ID)>();
	}
};
struct AtlasEntity : AtlasEntityMinimal
{
	
	std::vector<uint8_t> Metadata;

	void Serialize(ByteWriter& bw) const override
	{
		AtlasEntityMinimal::Serialize(bw);

		bw.blob(Metadata);
	}
	void Deserialize(ByteReader& br) override
	{
		AtlasEntityMinimal::Deserialize(br);
		Metadata.clear();
		const auto metadata_blob = br.blob();
		std::copy(metadata_blob.begin(),metadata_blob.end(),Metadata.begin());

	}
};