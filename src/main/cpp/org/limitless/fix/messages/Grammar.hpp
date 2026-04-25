//
// Created by Fredrik Dahlberg on 2026-04-25.
//

#ifndef SIMD_FIX_GRAMMAR_HPP
#define SIMD_FIX_GRAMMAR_HPP

#include "org/limitless/fix/parser/Dictionary.hpp"

namespace org::limitless::fix::generated {

inline constexpr std::array<messages::Dictionary, 8> TokenMeta{
    {
        {1, 0, false}, // add required and optional
        {10, 0, true},
        {49, 12, true},
        {102, 24, true},
        {627, 10, false},
        {628, 10, false},
        {629, 10, false},
        {630, 10, false}
        // ... total of 64 fields
    }
};
}

#endif //SIMD_FIX_GRAMMAR_HPP
