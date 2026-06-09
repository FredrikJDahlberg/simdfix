//
// Created by Fredrik Dahlberg on 2026-04-28.
//

#ifndef SIMD_FIX_TOKEN_HPP
#define SIMD_FIX_TOKEN_HPP

#include <cstdint>

struct Token
{
    uint16_t m_position;
    uint16_t m_tag;
    uint16_t m_length;
};

#endif //SIMD_FIX_TOKEN_HPP
