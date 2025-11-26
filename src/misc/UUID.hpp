#pragma once
#include "pch.hpp"
using UUID = boost::uuids::uuid;
class UUIDGen
{
    static int base20_value(char c)
    {
        for (int i = 0; i < 20; i++)
            if (BASE20_ALPHABET[i] == c)
                return i;
        throw std::runtime_error("Invalid Base20 char");
    }

public:
    static UUID Gen()
    {
        static boost::uuids::random_generator generator;
        return generator();
    }
    static const char inline BASE20_ALPHABET[21] ="0123456789ABCDEFGHJK"; // 20 chars, no I,L,O

    static std::string encode_base20(const UUID &uuid)
    {
        uint8_t data[16];
        memcpy(data, uuid.data, 16);

        __uint128_t val = 0;

        // Load 128-bit UUID into integer
        for (int i = 0; i < 16; i++)
            val = (val << 8) | data[i];

        // Encode into 31 Base20 characters
        std::string out(31, '0');
        for (int i = 30; i >= 0; i--)
        {
            out[i] = BASE20_ALPHABET[val % 20];
            val /= 20;
        }
        return out;
    }

    static UUID decode_base20(const std::string &str)
    {
        if (str.size() != 31)
            throw std::runtime_error("Invalid length (must be 31)");

        __uint128_t val = 0;

        // Convert from Base20
        for (char c : str)
            val = val * 20 + base20_value(c);

        // Unpack to 16 bytes
        UUID id;
        for (int i = 15; i >= 0; i--)
        {
            id.data[i] = (uint8_t)(val & 0xFF);
            val >>= 8;
        }

        return id;
    }
};