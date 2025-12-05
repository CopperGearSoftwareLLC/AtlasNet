#include "InterlinkPacket.hpp"

void InterlinkIdentityPacket::Serialize(std::vector<std::byte> &data) const
{
    std::string str = id.ToString();
    data.resize(str.size()+1);
    std::memcpy(data.data(),str.data(),str.size()+1);
}

bool InterlinkIdentityPacket::Deserialize(const std::vector<std::byte> &data)
{
    std::string str;
    str.resize(data.size());
    std::memcpy(str.data(),data.data(),data.size());
    id = InterLinkIdentifier::FromString(str).value();
    return true;
}
