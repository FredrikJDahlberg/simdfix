//
// Created by Fredrik Dahlberg on 2026-04-28.
//

#ifndef SIMD_FIX_GRAMMAR_HPP
#define SIMD_FIX_GRAMMAR_HPP

#include "org/limitless/fix/parser/Dictionary.hpp"
#include "../utils/PerfectHashMap.hpp"

namespace org::limitless::fix::protocols {

struct Logon
{
    // FIXME: generate
    static constexpr uint16_t arr[] =
    {
    1,
    34,
    49,
    102,
    627,
    628,
    629,
    630,
    };
    static constexpr std::array<Entry<parser::Dictionary>, 9> Grammar
    {
        {
            {1, {1, 0, false}},
            {34, {34, 0, true}},
            {49, {49, 12, true}},
            {102, {102, 24, true}},
            {627, {627, 10, false}},
            {628, {628, 10, false}},
            {629, {629, 10, false}},
            {630, {630, 10, false}}
        }
    };
};

struct Logout
{
    static constexpr std::array<Entry<parser::Dictionary>, 9> Grammar
    {
        {
            {1, {1, 0, false}},
            {10, {10, 0, true}},
            {34, {34, 0, true}},
            {49, {49, 12, true}},
            {102, {102, 24, true}},
            {627, {627, 10, false}},
            {628, {628, 10, false}},
            {629, {629, 10, false}},
            {630, {630, 10, false}}
        }
    };
};

struct HopGroup
{
    static constexpr std::array<Entry<parser::Dictionary>, 4> Grammar
    {
        {
            {627, {627, 10, false}},
            {628, {628, 10, false}},
            {629, {629, 10, false}},
            {630, {630, 10, false}}
        }
    };
};
}

#endif //SIMD_FIX_GRAMMAR_HPP
