#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <pch.hpp>
#include <string>

#include "NetworkEnums.hpp"
#include "Misc/UUID.hpp"
#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"

struct NetworkIdentity
{
	NetworkIdentityType Type = NetworkIdentityType::eInvalid;
	UUID ID;
	//std::string ID = "";

   public:
	NetworkIdentity() = default;
	NetworkIdentity(NetworkIdentityType _Type, const UUID& _ID = UUID()) : Type(_Type), ID(_ID) {}

	static NetworkIdentity MakeIDWatchDog();
	static NetworkIdentity MakeIDShard(const UUID& _ID);
	static NetworkIdentity MakeIDGameServer(const UUID& _ID);
	static NetworkIdentity MakeIDGameClient(const UUID& _ID);
	static NetworkIdentity MakeIDCartograph();

	std::string ToString() const;
	std::array<std::byte, 32> ToEncodedByteStream() const;
	//[[nodiscard]] static std::optional<InterLinkIdentifier> FromString(const std::string& input);
	/*
	[[nodiscard]] static std::optional<InterLinkIdentifier> FromEncodedByteStream(
		const std::array<std::byte, 32>& input);
	[[nodiscard]] static std::optional<InterLinkIdentifier> FromEncodedByteStream(
		const std::byte* data, size_t size);*/

	bool operator==(const NetworkIdentity& other) const noexcept

	{
		return ToString() == other.ToString();
	}
	bool operator<(const NetworkIdentity& other) const noexcept
	{
		return ToString() < other.ToString();
	}
	void Serialize(ByteWriter& bw) const
	{
		bw.write_scalar(Type);
		bw.uuid(ID);
	}
	void Deserialize(ByteReader& br)
	{
		Type = br.read_scalar<NetworkIdentityType>();
		ID = br.uuid();
	}
	[[nodiscard]] constexpr bool IsInternal() const {return Type != NetworkIdentityType::eGameClient;}
};
namespace std
{
template <>
struct hash<NetworkIdentity>
{
    size_t operator()(const NetworkIdentity& key) const noexcept
    {
        size_t h1 = std::hash<int>{}(static_cast<int>(key.Type));

        // treat the UUID as a contiguous byte array
        size_t h2 = std::hash<std::string_view>{}(
            std::string_view(reinterpret_cast<const char*>(key.ID.data()), key.ID.size())
        );

        // standard hash combine
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};
}