//
// Created by Fredrik Dahlberg on 2026-04-28.
//

#ifndef SIMD_FIX_GRAMMAR_HPP
#define SIMD_FIX_GRAMMAR_HPP

#include "org/limitless/fix/decoder/Dictionary.hpp"

namespace org::limitless::fix::protocols {

struct Logon
{
    static constexpr std::array<uint16_t, 14> Tags = {1, 8, 9, 10, 34, 35, 49, 52, 56, 102, 627, 628, 629, 630 };

    static constexpr std::array<decoder::Dictionary, 14> Grammar
    {
        {
            {1,   0,  decoder::Presence::Optional},
            {8,   0,  decoder::Presence::Required},
            {9,   0,  decoder::Presence::Required},
            {10,  0,  decoder::Presence::Required},
            {34,  0,  decoder::Presence::Required},
            {35,  0,  decoder::Presence::Required},
            {49,  12, decoder::Presence::Required},
            {52,  0,  decoder::Presence::Required},
            {56,  0,  decoder::Presence::Required},
            {102, 24, decoder::Presence::Required},
            {627, 10, decoder::Presence::Optional},
//            {628, 10, decoder::Presence::Optional},
//            {629, 10, decoder::Presence::Optional},
//            {630, 10, decoder::Presence::Optional}
        }
    };
};

struct Logout
{
    static constexpr std::array<uint16_t, 14> Tags = { 8, 9, 10, 34, 35, 49, 52, 56, 102, 627, 628, 629, 630 };

    static constexpr std::array<decoder::Dictionary, 14> Grammar
    {
        {
            {8,   0,  decoder::Presence::Required},
            {9,   0,  decoder::Presence::Required},
            {10,  1,  decoder::Presence::Required},
            {34,  0,  decoder::Presence::Required},
            {35,  0,  decoder::Presence::Required},
            {49,  0,  decoder::Presence::Required},
            {52,  0,  decoder::Presence::Required},
            {56,  0,  decoder::Presence::Required},
            {102, 0,  decoder::Presence::Required},
            {627, 10, decoder::Presence::Optional},
            {628, 10, decoder::Presence::Optional},
            {629, 10, decoder::Presence::Optional},
            {630, 10, decoder::Presence::Optional}
        }
    };
};

struct HopGroup
{
    static constexpr std::array<uint16_t, 4> Tags = { 627, 628, 629, 630 };

    static constexpr std::array<decoder::Dictionary, 4> Grammar
    {
        {
            {627, 10, decoder::Presence::Optional},
            {628, 10, decoder::Presence::Optional},
            {629, 10, decoder::Presence::Optional},
            {630, 10, decoder::Presence::Optional}
        }
    };
};
}

#endif //SIMD_FIX_GRAMMAR_HPP
