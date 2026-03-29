
#pragma once


#define AN_ASSERT(expr, msg) \
    do { \
        if (!(expr)) { \
            throw std::runtime_error(std::string("Assertion failed: ") + (msg)); \
        } \
    } while (0)