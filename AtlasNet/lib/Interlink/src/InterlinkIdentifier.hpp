#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <pch.hpp>
#include <string>

#include "InterlinkEnums.hpp"
#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"

struct InterLinkIdentifier
{
	InterlinkType Type = InterlinkType::eInvalid;
	static_string<64> ID;
	//std::string ID = "";

   public:
	InterLinkIdentifier() = default;
	InterLinkIdentifier(InterlinkType _Type, const std::string& _ID = "") : Type(_Type), ID(_ID) {}

	static InterLinkIdentifier MakeIDWatchDog();
	static InterLinkIdentifier MakeIDShard(const std::string& _ID);
	static InterLinkIdentifier MakeIDGameServer(const std::string& _ID);
	static InterLinkIdentifier MakeIDGameClient(const std::string& _ID);
	static InterLinkIdentifier MakeIDGameCoordinator();
	static InterLinkIdentifier MakeIDCartograph();

	std::string ToString() const;
	std::array<std::byte, 32> ToEncodedByteStream() const;
	[[nodiscard]] static std::optional<InterLinkIdentifier> FromString(const std::string& input);
	[[nodiscard]] static std::optional<InterLinkIdentifier> FromEncodedByteStream(
		const std::array<std::byte, 32>& input);
	[[nodiscard]] static std::optional<InterLinkIdentifier> FromEncodedByteStream(
		const std::byte* data, size_t size);

	bool operator==(const InterLinkIdentifier& other) const noexcept

	{
		return ToString() == other.ToString();
	}
	bool operator<(const InterLinkIdentifier& other) const noexcept
	{
		return ToString() < other.ToString();
	}
	void Serialize(ByteWriter& bw) const
	{
		bw.write_scalar(Type);
		bw.str(ID);
	}
	void Deserialize(ByteReader& br)
	{
		Type = br.read_scalar<InterlinkType>();
		ID = br.str();
	}
};
namespace std
{
template <>
struct hash<InterLinkIdentifier>
{
    size_t operator()(const InterLinkIdentifier& key) const noexcept
    {
        size_t h1 = std::hash<int>{}(static_cast<int>(key.Type));
        size_t h2 = std::hash<std::string_view>{}(
            std::string_view(key.ID.data(), key.ID.size())
        );

        // standard hash combine
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};
}
