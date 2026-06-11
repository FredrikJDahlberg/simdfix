//
// Created by Fredrik Dahlberg on 2026-04-23.
//

#ifndef SIMD_FIX_UTILS_HPP
#define SIMD_FIX_UTILS_HPP

#include <cstdint>
#include <string_view>
#include <ranges>
#include <chrono>

namespace org::limitless::fix::utils {

using String = std::span<const uint8_t>;

inline void print(const uint32_t length, const uint8_t* buffer)
{
    for (uint32_t i = 0; i < length; ++i)
    {
        const auto ch = buffer[i];
        std::printf("%2c ", std::isprint(ch) ? ch : ch == 1 ? '|' : '?');
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

// `digits` must have at least sizeof(uint64_t) readable bytes; bytes beyond
// `length` are shifted out and do not affect the result.
[[nodiscard]] inline uint32_t binaryToDecimal(const uint32_t value, const uint8_t* digits, const uint32_t length)
{
    uint64_t number = 0;
    if (length != 0)
    {
        memcpy(&number, digits, sizeof(uint64_t));
        number <<= (sizeof(uint64_t) - length) * 8;
    }
    number = number * 10 + (number >> 8); // val = (val * 2561) >> 8;
    number = ((number & SwarMask) * SwarFactor1 + (number >> 16 & SwarMask) * SwarFactor2) >> 32;
    return scale(value, length) + number;
}

inline constexpr uint64_t AsciiZeros = 0x3030303030303030ULL;

// digits,  must have at least sizeof(uint64_t) readable bytes; bytes beyond
// length,  are shifted out and do not affect the result.
template <typename CharType>
[[nodiscard]] uint64_t asciiToUint64(const CharType* digits, const uint32_t length, const bool padded)
{
    static_assert(sizeof(CharType) == 1, "asciiToUint64 only accepts 1-byte data type arrays.");

    if (length == 0)
    {
        return 0;
    }
    uint64_t number = 0;
    if (padded)
    {
        const uint32_t shift = (sizeof(uint64_t) - std::min(length, 8U)) * 8;
        uint64_t raw;
        memcpy(&raw, digits, sizeof(raw));
        number = (raw << shift) | (AsciiZeros & ((1ULL << shift) - 1));
        number -= AsciiZeros;
        number = number * 10 + (number >> 8); // val = (val * 2561) >> 8;
        number = ((number & SwarMask) * SwarFactor1 + (number >> 16 & SwarMask) * SwarFactor2) >> 32;
    }
    else
    {
        for (uint32_t i = 0; i < length; ++i)
        {
            number *= 10;
            number += digits[i] - '0';
        }
    }
    return number;
}

template <typename T>
[[nodiscard]] uint64_t asciiToUint64(const uint64_t value, const T* digits, const uint32_t length, const bool padded)
{
    return scale(value, length) + asciiToUint64(digits, length, padded);
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
    bytes = bytes - 0x01010101'01010101ULL & ~bytes & 0x80808080'80808080ULL;
    return bytes;
}

constexpr uint64_t littleEndianUint64(const std::string_view str)
{
    const size_t n = std::min(8zu, str.size());
    uint64_t result = 0;
    for (size_t i = 0; i < n; ++i)
    {
        result |= static_cast<uint64_t>(static_cast<uint8_t>(str[i])) << (i * 8);
    }
    return result;
}

[[nodiscard]] inline uint32_t fastDivide100(const uint32_t number) {
    return static_cast<uint32_t>(number * 0x51EB851FULL >> 37);
}

[[nodiscard]] inline uint32_t fastModulo100(const uint32_t number)
{
    return number - fastDivide100(number) * 100;
}

[[nodiscard]] inline uint32_t fastDivide10000(const uint32_t number) {
    return static_cast<uint32_t>(number * 0xD1B71759ULL >> 45);
}

[[nodiscard]] inline uint32_t fastModulo10000(const uint32_t number) {
    return number - fastDivide10000(number) * 10000;
}

[[nodiscard]] inline int64_t daysSince1970(int year, int month, int day) noexcept
{
    // Shift the calendar so that March is the first month of the "built-in" year.
    // This trick moves the leap day (Feb 29) to the very end of the calculation loop,
    // making the leap year distribution perfectly linear without if/else blocks.
    year -= (month <= 2) ? 1 : 0;

    // Calculate historical leap days using the standard formula layout
    const int64_t era = fastDivide100 (year >= 0 ? year : year - 399) >> 2;
    const int64_t yoe = static_cast<uint32_t>(year - era * 400);            // Year of era
    const int64_t doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1; // Day of year
    const int64_t doe = yoe * 365 + yoe / 4 - fastDivide100(yoe) + doy;              // Day of era

    // Subtract the exact day count offset for January 1st, 1970
    return era * 146097 + doe - 719468;
}

inline int64_t dateTimeToEpochUTC(const uint8_t* data, const uint32_t length)
{
    static constexpr uint64_t MillisPerDay = 24 * 60 * 60 * 1'000;
    if (length < 17)
    {
        return -1;
    }
    const uint64_t date = asciiToUint64(data, 8, true);
    const auto years = fastDivide10000(date);
    const auto month = fastDivide100(fastModulo10000(date));
    const auto day = fastModulo100(date);
    const auto days = daysSince1970(years, month, day);

    uint64_t time = 0;
    memcpy(&time, data + 9, sizeof(time));
    time -= 0x30303a30303a3030ull;
    const auto hours = (time & 0xff) * 10 + ((time >> 8) & 0xff);
    const auto mins  = (time >> 24 & 0xff) * 10 + (time >> 32 & 0xff);
    const auto secs  = (time >> 48 & 0xff) * 10 + (time >> 56);
    uint64_t millis = days * MillisPerDay + (hours * 3'600 + mins * 60 + secs) * 1000;
    if (length == 17)
    {
        return millis;
    }
    if (length == 21)
    {
        time = 0;
        memcpy(&time, data + 17, 4);
        time -= 0x3030302e;
        millis += (time >> 24) + (time >> 16 & 0xff) * 10 + (time >> 8 & 0xff) * 100;
        return millis;
    }
    return -1;
}

inline std::chrono::milliseconds dateTimeToEpochUTC(const std::string_view dateTime)
{
    const auto data = reinterpret_cast<const uint8_t*>(dateTime.data());
    return std::chrono::milliseconds{dateTimeToEpochUTC(data, dateTime.length())};
}

template <typename Enum>
Enum find(const uint8_t code)
{
    const auto end = Enum::Codes + std::size(Enum::Codes);
    const auto found = std::find(Enum::Codes, end, code);
    Enum value{};
    if (found != end)
    {
        auto index = std::distance(Enum::Codes, found);
        value.m_value = static_cast<Enum::Values>(index);
    }
    return value;
}

}

#endif //SIMD_FIX_UTILS_HPP
