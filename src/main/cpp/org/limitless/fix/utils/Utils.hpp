//
// Created by Fredrik Dahlberg on 2026-04-23.
//

#ifndef SIMD_FIX_UTILS_HPP
#define SIMD_FIX_UTILS_HPP

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

//
// See https://lemire.me/blog/2022/01/21/swar-explained-parsing-eight-digits/
//
[[nodiscard]] inline uint32_t asciiToDecimal(const uint8_t* digits, const uint32_t length)
{
    constexpr uint64_t SwarMask = 0x000000FF000000FF;
    constexpr uint64_t SwarFactor1 = 0x000F424000000064; // 100 + (1000000ULL << 32)
    constexpr uint64_t SwarFactor2 = 0x0000271000000001; // 1 + (10000ULL << 32)
    uint64_t number = 0x3030303030303030;
    memcpy(reinterpret_cast<uint8_t*>(&number) + sizeof(uint64_t) - length, digits, length);
    number -= 0x3030303030303030;
    number = number * 10 + (number >> 8); // val = (val * 2561) >> 8;
    number = ((number & SwarMask) * SwarFactor1 + (number >> 16 & SwarMask) * SwarFactor2) >> 32;
    return number;
}

[[nodiscard]] inline uint32_t scale(uint32_t value, const uint32_t power)
{
    static constexpr uint32_t Powers10[] =
    {
        1ul,
        10ul,
        100ul,
        1'000ul,
        10'000ul,
        100'000ul,
        1'000'000ul,
        10'000'000ul,
        100'000'000ul,
        1'000'000'000ul,
    };
    return value * Powers10[power];
}

[[nodiscard]] inline uint32_t asciiToDecimal(const uint32_t value, const uint8_t* digits, const uint32_t length)
{
    return scale(value, length) + asciiToDecimal(digits, length);
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

constexpr uint64_t littleEndianUint64(std::string_view str)
{
    const size_t n = str.size() > 8 ? 8 : str.size();
    uint64_t result = 0;
    for (size_t i = 0; i < n; ++i)
    {
        result |= (static_cast<uint64_t>(static_cast<uint8_t>(str[i])) << (i * 8));
    }
    return result;
}
}

#endif //SIMD_FIX_UTILS_HPP
