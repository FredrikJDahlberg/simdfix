//
// Created by Fredrik Dahlberg on 2026-04-28.
//

#ifndef SIMD_FIX_TOKEN_HPP
#define SIMD_FIX_TOKEN_HPP

#include <cstdint>

struct Token
{
    uint32_t position;
    uint16_t tag;
    uint16_t length;

    [[nodiscard]] uint32_t end() const
    {
        return position + length + 1;
    }
};

#endif //SIMD_FIX_TOKEN_HPP
