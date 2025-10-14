#pragma once
#include <pch.hpp>
#include "InterlinkEnums.hpp"
#include <cstdint>
#include <string>
#include <optional>
#include <functional>


struct InterLinkIdentifier
{
	InterlinkType Type = InterlinkType::eInvalid;
	std::string ID = "";

public:
	InterLinkIdentifier() = default;
	InterLinkIdentifier(InterlinkType _Type, const std::string& _ID = "")
		: Type(_Type), ID(_ID) {}

	static InterLinkIdentifier MakeIDGod();
	static InterLinkIdentifier MakeIDPartition(const std::string& _ID);
	static InterLinkIdentifier MakeIDGameServer(const std::string& _ID);
	static InterLinkIdentifier MakeIDGameClient(const std::string& _ID);
	static InterLinkIdentifier MakeIDGodView();

	std::string ToString() const;
	std::array<std::byte,32> ToEncodedByteStream() const;
	[[nodiscard]] static std::optional<InterLinkIdentifier> FromString(const std::string& input);
	[[nodiscard]] static std::optional<InterLinkIdentifier> FromEncodedByteStream(const std::array<std::byte,32>& input);
	[[nodiscard]] static std::optional<InterLinkIdentifier> FromEncodedByteStream(const std::byte *data, size_t size);

	bool operator==(const InterLinkIdentifier& other) const noexcept
	{
		return Type == other.Type && ID == other.ID;
	}
	  bool operator<(const InterLinkIdentifier& other) const noexcept
    {
        return std::tie(Type, ID) < std::tie(other.Type, other.ID);
    }
};

namespace std {
template<>
struct hash<InterLinkIdentifier>
{
    size_t operator()(const InterLinkIdentifier& key) const noexcept
    {
        return std::hash<int>()(static_cast<int>(key.Type)) ^ std::hash<std::string>()(key.ID);
    }
};
}
