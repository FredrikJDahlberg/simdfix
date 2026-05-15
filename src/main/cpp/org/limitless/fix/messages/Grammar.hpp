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
    static constexpr std::array<uint16_t, 14> Tags = {1, 8, 9, 10, 34, 35, 49, 52, 56, 102, 627, 628, 629, 630 };

    static constexpr std::array<parser::Dictionary, 14> Grammar
    {
        {
            {1, 0, false},
            {8, 0, true},
            {9, 0, true},
            {10, 0, true},
            {34, 0, true},
            {35, 0, true},
            {49, 12, true},
            {52, 0, true},
            {56, 0, true},
            {102, 24, true},
            {627, 10, false},
            {628, 10, false},
            {629, 10, false},
            {630, 10, false}
        }
    };
};

struct Logout
{
    static constexpr std::array<uint16_t, 14> Tags = { 8, 9, 10, 34, 35, 49, 52, 56, 102, 627, 628, 629, 630 };

    static constexpr std::array<parser::Dictionary, 14> Grammar
    {
            {  // 0 0 0 0 0 0 10 10 10 1
                {8, 0, true},
                {9, 0, true},
                {10, 1, true},
                {34, 0, true},
                {35, 0, true},
                {49, 0, true},
                {52, 0, true},
                {56, 0, true},
                {102, 0, true},
                {627, 10, false},
                {628, 10, false},
                {629, 10, false},
                {630, 10, false}
            }
    };
};

struct HopGroup
{
    static constexpr std::array<uint16_t, 4> Tags = { 627, 628, 629, 630 };

    static constexpr std::array<parser::Dictionary, 4> Grammar
    {
        {
            {627, 10, false},
            {628, 10, false},
            {629, 10, false},
            {630, 10, false}
        }
    };
};
}

#endif //SIMD_FIX_GRAMMAR_HPP
