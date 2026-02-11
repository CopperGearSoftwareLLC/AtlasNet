#pragma once
#include "Network/IPAddress.hpp"
#include "Misc/UUID.hpp"
class Client{

    IPAddress ip;
    UUID ID;

    public:
    static Client MakeNewClient(UUID id, IPAddress ip)
    {
        Client c;
        c.ID = UUIDGen::Gen();
        c.ip = ip;
        return c;
    }
};