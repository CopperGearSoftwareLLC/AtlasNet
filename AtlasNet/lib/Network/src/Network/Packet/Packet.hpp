#pragma once

/// @brief Packet Types Internal to InterLink
#include "Misc/Singleton.hpp"
#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"
#include "pch.hpp"
/*
enum class PacketType
{
    eDummy,
	eInvalid,
	eRelay ,
	eCommand,

    //Event System
    eEventSystemRequest,
};
BOOST_DESCRIBE_ENUM(PacketType, eInvalid, eRelay,eCommand,eEventSystemRequest)*/
using PacketTypeID = uint32_t;

class IPacket
{
	PacketTypeID packet_type = -1;

   public:
	IPacket(PacketTypeID t) : packet_type(t) {}
	virtual ~IPacket() = default;
	IPacket& SetPacketType(PacketTypeID t)
	{
		packet_type = t;
		return *this;
	}
    virtual const std::string_view GetPacketName() const = 0;
	PacketTypeID GetPacketType() const { return packet_type; }
	const IPacket& Serialize(ByteWriter& bw) const;
	IPacket& Deserialize(ByteReader& br);
	[[nodiscard]] bool Validate() const;

   protected:
	virtual void SerializeData(ByteWriter& bw) const = 0;
	virtual void DeserializeData(ByteReader& br) = 0;
	[[nodiscard]] virtual bool ValidateData() const = 0;
};
class PacketRegistry : public Singleton<PacketRegistry>
{
public:
    using FactoryFn = std::unique_ptr<IPacket>(*)();

    static PacketRegistry& Instance()
    {
        static PacketRegistry instance;
        return instance;
    }

    bool Register(PacketTypeID type, FactoryFn fn)
    {
        return factories.emplace(type, fn).second;
    }

    std::unique_ptr<IPacket> Create(PacketTypeID type) const
    {
        auto it = factories.find(type);
        if (it == factories.end())
		{
			ASSERT(false, "Packet Type not registered?");
            return nullptr;

		}
        return it->second();
    }

    std::unique_ptr<IPacket> CreateFromBytes(std::span<const uint8_t> bytes) const
    {
        ByteReader br(bytes);

        PacketTypeID type = br.read_scalar<PacketTypeID>();
        ByteReader br2(bytes);
        auto pkt = Create(type);
        if (!pkt)
            return nullptr;

        pkt->Deserialize(br2);

        if (!pkt->Validate())
		{
			ASSERT(false,"Message From bytes failed to validate");
            return nullptr;

		}

        return pkt;
    }

private:
    std::unordered_map<PacketTypeID, FactoryFn> factories;
};
template <size_t N>
struct FixedString
{
    char value[N];

    constexpr FixedString(const char (&str)[N])
    {
        for (size_t i = 0; i < N; ++i)
            value[i] = str[i];
    }
};
static constexpr uint32_t HashString(const char* str)
{
    uint32_t hash = 2166136261u;
    while (*str)
    {
        hash ^= static_cast<uint8_t>(*str++);
        hash *= 16777619u;
    }
    return hash;
}
template <typename Derived, FixedString Name>
class TPacket : public IPacket
{
    public:
    static constexpr PacketTypeID TypeID =
        HashString(Name.value);
protected:


    TPacket() : IPacket(TypeID) {}

public:
    static std::unique_ptr<IPacket> Create()
    {
        return std::make_unique<Derived>();
    }
    const std::string_view GetPacketName() const override 
    {
        return Name.value;
    }
};

#define ATLASNET_REGISTER_PACKET(Type, Name)                     \
    static const bool Type##_registered = []() -> bool {         \
        PacketRegistry::Get().Register(HashString(Name),         \
                                       &Type::Create);           \
        return true;                                             \
    }()