#include "InterlinkIdentifier.hpp"
#include "InterlinkEnums.hpp"

InterLinkIdentifier InterLinkIdentifier::MakeIDGod()

{
    InterLinkIdentifier id;
    id.Type = InterlinkType::eGod;
    id.ID = "";
    return id;
}

InterLinkIdentifier InterLinkIdentifier::MakeIDPartition(const std::string &_ID)
{
    InterLinkIdentifier id;
    id.Type = InterlinkType::ePartition;
    id.ID = _ID;
    return id;
}

InterLinkIdentifier InterLinkIdentifier::MakeIDGameServer(const std::string &_ID)
{
    InterLinkIdentifier id;
    id.Type = InterlinkType::eGameServer;
    id.ID = _ID;
    return id;
}

InterLinkIdentifier InterLinkIdentifier::MakeIDGameClient(const std::string &_ID)
{
    InterLinkIdentifier id;
    id.Type = InterlinkType::eGameClient;
    id.ID = _ID;
    return id;
}

InterLinkIdentifier InterLinkIdentifier::MakeIDGodView()
{
    InterLinkIdentifier id;
    id.Type = InterlinkType::eGameClient;
    id.ID = "";
    return id;
}

std::string InterLinkIdentifier::ToString() const
{
    return boost::describe::enum_to_string(Type, "UnknownUnknownType?") + std::string(" ") + (ID);
}

std::array<std::byte, 32> InterLinkIdentifier::ToEncodedByteStream() const
{
    std::array<std::byte, 32> result{};
    result[0] = (std::byte)Type;
    ASSERT(ID.size() <= 31, "ID too long to insert into encoded bytestream");
    for (int i = 0; i < ID.size(); i++)
    {
        result[i + 1] = (std::byte)ID[i];
    }
    return result;
}
std::optional<InterLinkIdentifier> InterLinkIdentifier::FromEncodedByteStream(const std::array<std::byte, 32> &input)
{

    return FromEncodedByteStream(input.data(), input.size());
}

std::optional<InterLinkIdentifier> InterLinkIdentifier::FromEncodedByteStream(const std::byte *data, size_t size)
{
    ASSERT(size <= 32, "Invalid Byte Stream size");
    InterLinkIdentifier ID;
    ID.Type = (InterlinkType)data[0];
    ID.ID.resize(32);
    for (int i = 0; i < 31; i++)
    {
        ID.ID[i] = (char)data[i + 1];
    }
    ID.ID.push_back(0);
    ID.ID.shrink_to_fit();
    return ID;
}

std::optional<InterLinkIdentifier> InterLinkIdentifier::FromString(const std::string &input)
{
    std::istringstream iss(input);
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

    InterLinkIdentifier id;

    bool success = boost::describe::enum_from_string(enumName.c_str(), id.Type);
    if (!success)
    {
        std::cerr << "unable to parse " << input << std::endl;
        return std::nullopt;
    }

    // If only one token was present (like "eGod"), make ID empty
    if (idValue.empty())
    {
        id.ID.clear();
    }
    else
    {
        id.ID = idValue;
    }

    return id;
}
