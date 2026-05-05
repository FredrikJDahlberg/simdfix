//
// Created by Fredrik Dahlberg on 2026-04-23.
//

#ifndef SIMD_FIX_UTILS_HPP
#define SIMD_FIX_UTILS_HPP

#include <cstdint>

namespace org::limitless::fix::parser {

inline void print(const uint32_t length, const uint8_t* buffer)
{
    for (uint32_t i = 0; i < length; ++i)
    {
        const auto ch = buffer[i];
        std::printf("%2c ", std::isprint(ch) ? ch : (ch == 1 ? '|' : '?'));
    }
    std::printf("\n");
}

[[nodiscard]] inline uint32_t binaryToDecimal(uint32_t value, const uint8_t* digits, const uint32_t length)
{
    if (length >= 5)
    {
        for (uint32_t position = 0; position < length; ++position)
        {
            value = value * 10 + digits[position];
        }
    }
    else
    {
        if (length >= 1)
        {
            value = value * 10 + digits[0];
        }
        if (length >= 2)
        {
            value = value * 10 + digits[1];
        }
        if (length >= 3)
        {
            value = value * 10 + digits[2];
        }
        if (length >= 4)
        {
            value = value * 10 + digits[3];
        }
    }
    return value;
}

inline uint32_t asciiToDecimal(uint32_t value, const uint8_t* digits, const uint32_t length)
{
    if (length >= 5)
    {
        for (uint32_t position = 0; position < length; ++position)
        {
            value = value * 10 + digits[position] - '0';
        }
    }
    else
    {
        if (length >= 1)
        {
            value = value * 10 + digits[0] - '0';
        }
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
    }
    return value;
}


template<size_t N>
[[nodiscard]] constexpr auto makeSpan(const char (&str)[N]) noexcept
{
    return std::span<const uint8_t, N - 1>(reinterpret_cast<const uint8_t*>(str), N - 1);
}

[[nodiscard]] inline uint64_t findByte(const uint8_t value, uint64_t bytes)
{
    const uint64_t mask = 0x01010101'01010101ULL*value;
    bytes ^= mask;
    bytes = (bytes - 0x01010101'01010101ULL) & ~bytes & 0x80808080'80808080ULL;
    return bytes;
}

[[nodiscard]] inline uint32_t nextPosition(uint64_t& mask)
{
    const uint32_t position = std::countr_zero(mask);
    mask &= ~(1ULL << position);
    return position / 8;
};

constexpr uint64_t littleEndianUint64(std::string_view str)
{
    size_t n = str.size() > 8 ? 8 : str.size();
    uint64_t result = 0;
    for (size_t i = 0; i < n; ++i)
    {
        result |= (static_cast<uint64_t>(static_cast<uint8_t>(str[i])) << (i * 8));
    }
    return result;
}

}

#endif //SIMD_FIX_UTILS_HPP
