#include "Packet.hpp"


IPacket& IPacket::Deserialize(ByteReader& br) 
{
    packet_type = br.read_scalar<decltype(packet_type)>();
    DeserializeData(br);
    return *this;
}
const IPacket& IPacket::Serialize(ByteWriter& bw) const 
{
    bw.write_scalar<decltype(packet_type)>(packet_type);
    SerializeData(bw);
    return *this;
}
bool IPacket::Validate() const
{
	return (packet_type != PacketType::eInvalid) && ValidateData();
}
