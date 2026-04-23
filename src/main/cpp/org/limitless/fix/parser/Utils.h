//
// Created by Fredrik Dahlberg on 2026-04-23.
//

#ifndef SIMD_FIX_UTILS_H
#define SIMD_FIX_UTILS_H

#include "org/limitless/fix/parser/Tokenizer.hpp"

namespace org::limitless::fix::parser {

void print(const uint32_t length, const uint8_t* buffer)
{
    for (uint32_t i = 0; i < length; ++i)
    {
        const auto ch = buffer[i];
        std::printf("%2c ", std::isprint(ch) ? ch : (ch == 1 ? '|' : '?'));
    }
    std::printf("\n");
}

[[nodiscard]] uint32_t convertToDecimal(uint32_t value, const uint8_t* digits, const int32_t count)
{
    if (count >= 1)
    {
        value = value * 10 + digits[0];
    }
    if (count >= 2)
    {
        value = value * 10 + digits[1];
    }
    if (count >= 3)
    {
        value = value * 10 + digits[2];
    }
    if (count >= 4)
    {
        value = value * 10 + digits[3];
    }
    return value;
}

uint32_t asciiToDecimal(const uint8_t* digits, const uint32_t length)
{
    uint32_t value = digits[0] - '0';
    if (length >= 2)
    {
        value = value * 10 + digits[1] - '0';
    }
    if (length >= 3)
    {
        value = value * 10 + digits[2] - '0';
    }
    if (length >= 4)
    {
        value = value * 10 + digits[3] - '0';
    }
    if (length >= 5)
    {
        value = value * 10 + digits[4] - '0';
    }
    if (length >= 6)
    {
        value = value * 10 + digits[5] - '0';
    }
    return value;
}

}

#endif //SIMD_FIX_UTILS_H
