#pragma once
#include "pch.hpp"
#include "misc/Singleton.hpp"
class SecretGenerator: public Singleton<SecretGenerator>
{
    public:
    SecretGenerator() = default;
    std::string GenerateUniqueSecret() const;
};