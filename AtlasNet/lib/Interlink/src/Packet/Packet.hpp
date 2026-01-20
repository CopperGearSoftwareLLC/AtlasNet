#pragma once

/// @brief Packet Types Internal to InterLink
#include "Misc/Singleton.hpp"
#include "Serialize/ByteReader.hpp"
#include "Serialize/ByteWriter.hpp"
#include "pch.hpp"
enum class PacketType
{
	eInvalid = 0,
	eRelay = 1,
	eCommand = 2,

    //Event System
    eEventSystemRequest = 3,
};
BOOST_DESCRIBE_ENUM(PacketType, eInvalid, eRelay,eCommand,eEventSystemRequest)

class IPacket
{
	PacketType packet_type = PacketType::eInvalid;

   public:
	IPacket(PacketType t) : packet_type(t) {}
	virtual ~IPacket() = default;
	IPacket& SetPacketType(PacketType t)
	{
		packet_type = t;
		return *this;
	}
	PacketType GetPacketType() const { return packet_type; }
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

    bool Register(PacketType type, FactoryFn fn)
    {
        return factories.emplace(type, fn).second;
    }

    std::unique_ptr<IPacket> Create(PacketType type) const
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

        PacketType type = br.read_scalar<PacketType>();
        auto pkt = Create(type);
        if (!pkt)
            return nullptr;

        pkt->Deserialize(br);

        if (!pkt->Validate())
		{
			ASSERT(false,"Message From bytes failed to validate");
            return nullptr;

		}

        return pkt;
    }

private:
    std::unordered_map<PacketType, FactoryFn> factories;
};

template <typename Derived, PacketType Type>
class TPacket : public IPacket
{
protected:
    TPacket() : IPacket(Type) {}

private:
    static std::unique_ptr<IPacket> Create()
    {
        return std::make_unique<Derived>();
    }

    struct Registrar
    {
        Registrar()
        {
            PacketRegistry::Get().Register(Type, &TPacket::Create);
        }
    };

    static inline Registrar registrar{};
};
