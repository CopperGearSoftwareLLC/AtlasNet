#pragma once

#include <cstddef>
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