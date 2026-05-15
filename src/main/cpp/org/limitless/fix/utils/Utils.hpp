//
// Created by Fredrik Dahlberg on 2026-04-23.
//

#ifndef SIMD_FIX_UTILS_HPP
#define SIMD_FIX_UTILS_HPP

#include <cstdint>

namespace org::limitless::fix::utils {

inline void print(const uint32_t length, const uint8_t* buffer)
{
    for (uint32_t i = 0; i < length; ++i)
    {
        const auto ch = buffer[i];
        std::printf("%2c ", std::isprint(ch) ? ch : (ch == 1 ? '|' : '?'));
    }
    std::printf("\n");
}

[[nodiscard]] inline uint32_t scale(const uint32_t value, const uint32_t power)
{
    switch (power)
    {
        case 0: return value;
        case 1: return value * 10;
        case 2: return value * 100;
        case 3: return value * 1'000;
        case 4: return value * 10'000;
        case 5: return value * 100'000;
        case 6: return value * 1'000'000;
        case 7: return value * 10'000'000;
        case 8: return value * 100'000'000;
        case 9: return value * 1'000'000'000;
        default: return value;

    }
}

[[nodiscard]] inline uint64_t scale(const uint64_t value, const uint32_t power)
{
    switch (power)
    {
        case 0: return value;
        case 1: return value * 10ULL;
        case 2: return value * 100ULL;
        case 3: return value * 1'000ULL;
        case 4: return value * 10'000ULL;
        case 5: return value * 100'000ULL;
        case 6: return value * 1'000'000ULL;
        case 7: return value * 10'000'000ULL;
        case 8: return value * 100'000'000ULL;
        case 9: return value * 1'000'000'000ULL;
        case 10: return value * 10'000'000'000ULL;
        case 11: return value * 100'000'000'000ULL;
        case 12: return value * 1'000'000'000'000ULL;
        case 13: return value * 10'000'000'000'000ULL;
        case 14: return value * 100'000'000'000'000ULL;
        case 15: return value * 1'000'000'000'000'000ULL;
        case 16: return value * 10'000'000'000'000'000ULL;
        case 17: return value * 100'000'000'000'000'000ULL;
        case 18: return value * 1'000'000'000'000'000'000ULL;
        case 19: return value * 10'000'000'000'000'000'000ULL;
        default: return value; // 18'446'744'073'709'551'615
    }
}

//
// See https://lemire.me/blog/2022/01/21/swar-explained-parsing-eight-digits/
//
inline constexpr uint64_t SwarMask = 0x000000FF000000FF;
inline constexpr uint64_t SwarFactor1 = 0x000F424000000064; // 100 + (1000000ULL << 32)
inline constexpr uint64_t SwarFactor2 = 0x0000271000000001; // 1 + (10000ULL << 32)

[[nodiscard]] inline uint32_t binaryToDecimal(const uint32_t value, const uint8_t* digits, const uint32_t length)
{
    uint64_t number = 0;
    memcpy(reinterpret_cast<uint8_t*>(&number) + sizeof(uint64_t) - length, digits, length);
    number = number * 10 + (number >> 8); // val = (val * 2561) >> 8;
    number = ((number & SwarMask) * SwarFactor1 + (number >> 16 & SwarMask) * SwarFactor2) >> 32;
    return scale(value, length) + number;
}

[[nodiscard]] inline uint64_t asciiToDecimal(const uint64_t value, const uint8_t* digits, const uint32_t length)
{
    uint64_t number = 0x3030303030303030;
    memcpy(reinterpret_cast<uint8_t*>(&number) + sizeof(uint64_t) - length, digits, length);
    number -= 0x3030303030303030;
    number = number * 10 + (number >> 8); // val = (val * 2561) >> 8;
    number = ((number & SwarMask) * SwarFactor1 + (number >> 16 & SwarMask) * SwarFactor2) >> 32;
    return scale(value, length) + number;
}

template<size_t N>
[[nodiscard]] constexpr auto makeSpan(const char (& str)[N]) noexcept
{
    return std::span<const uint8_t, N - 1>(reinterpret_cast<const uint8_t*>(str), N - 1);
}

[[nodiscard]] inline uint64_t findByte(const uint8_t value, uint64_t bytes)
{
    const uint64_t mask = 0x01010101'01010101ULL * value;
    bytes ^= mask;
    bytes = (bytes - 0x01010101'01010101ULL) & ~bytes & 0x80808080'80808080ULL;
    return bytes;
}

constexpr uint64_t littleEndianUint64(const std::string_view str)
{
    const size_t n = std::min(8zu, str.size());
    uint64_t result = 0;
    for (size_t i = 0; i < n; ++i)
    {
        result |= (static_cast<uint64_t>(static_cast<uint8_t>(str[i])) << (i * 8));
    }
    return result;
}
}

#endif //SIMD_FIX_UTILS_HPP
