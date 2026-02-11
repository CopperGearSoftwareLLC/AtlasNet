#include "NetworkIdentity.hpp"
#include "NetworkEnums.hpp"
#include "Misc/String_utils.hpp"
#include <boost/uuid/uuid_io.hpp>
#include <iostream>
#include "Misc/UUID.hpp"
#include "boost/uuid/name_generator.hpp"
NetworkIdentity NetworkIdentity::MakeIDWatchDog()

{
    NetworkIdentity id;
    id.Type = NetworkIdentityType::eWatchDog;
     id.ID = UUID();
    return id;
}

NetworkIdentity NetworkIdentity::MakeIDShard(const UUID &_ID)
{
    NetworkIdentity id;
    id.Type = NetworkIdentityType::eShard;
    id.ID = _ID;
    return id;
}

NetworkIdentity NetworkIdentity::MakeIDGameServer(const UUID &_ID)
{
    NetworkIdentity id;
    id.Type = NetworkIdentityType::eGameServer;
    id.ID = _ID;
    return id;
}

NetworkIdentity NetworkIdentity::MakeIDGameClient(const UUID &_ID)
{
    NetworkIdentity id;
    id.Type = NetworkIdentityType::eGameClient;
    id.ID = _ID;
    return id;
}



NetworkIdentity NetworkIdentity::MakeIDCartograph()
{
    NetworkIdentity id;
    id.Type = NetworkIdentityType::eCartograph;
    id.ID = UUID();
    return id;
}

std::string NetworkIdentity::ToString() const
{
    std::string type_string = boost::describe::enum_to_string(Type, "UnknownUnknownType?");
    if (!ID.is_nil())
    {

        type_string += std::string(" ") +boost::uuids::to_string(ID);
    }
    return NukeString(type_string);
}
/*
std::array<std::byte, 32> NetworkIdentity::ToEncodedByteStream() const
{
    std::array<std::byte, 32> result{std::byte(0)};
    result[0] = (std::byte)Type;
    if (!ID.empty())
    {
        ASSERT(ID.size() <= 31, "ID too long to insert into encoded bytestream");
        for (int i = 0; i < ID.size(); i++)
        {
            result[i + 1] = (std::byte)ID[i];
        }
    }
    else
    {
        result[1] = std::byte(0);
    }
    return result;
}

std::optional<NetworkIdentity> NetworkIdentity::FromEncodedByteStream(const std::array<std::byte, 32> &input)
{

    return FromEncodedByteStream(input.data(), input.size());
}

std::optional<NetworkIdentity> NetworkIdentity::FromEncodedByteStream(const std::byte *data, size_t size)
{

    ASSERT(size <= 32, "Invalid Byte Stream size");
    NetworkIdentity ID;
    ID.Type = (NetworkIdentityType)data[0];
    if ((char)data[1] != char(0))
    {
        
        ID.ID.resize(32);
        for (int i = 0; i < 31; i++)
        {
            ID.ID[i] = (char)data[i + 1];
        }
        ID.ID.push_back(0);
        ID.ID.shrink_to_fit();
    }
    else
    {
        ID.ID = UUID();
    }

    return ID;
}*/
/*
std::optional<NetworkIdentity> NetworkIdentity::FromString(const std::string &input)
{
    std::istringstream iss(NukeString(input));
    std::string enumName;
    std::string idValue;

    // Read the enum name first
    if (!(iss >> enumName))
    {
        std::cerr << "unable to parse " << input << std::endl;
        return std::nullopt;
    }

    // Try to read a second token (optional)
    iss >> idValue;

    NetworkIdentity id;

    bool success = boost::describe::enum_from_string(enumName.c_str(), id.Type);
    if (!success)
    {
        std::cerr << "unable to parse " << input << std::endl;
        return std::nullopt;
    }

    // If only one token was present (like "eGod"), make ID empty
    if (idValue.empty())
    {
        id.ID = UUID();
    }
    else
    {
        id.ID = NukeString(idValue);
    }

    return id;
}
*/