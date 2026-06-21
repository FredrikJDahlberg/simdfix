//
// Created by Fredrik Dahlberg on 2026-04-23.
//

#ifndef SIMD_FIX_UTILS_HPP
#define SIMD_FIX_UTILS_HPP

#include <cstdint>
#include <string_view>
#include <chrono>
#include <span>
#include <bit>
#include <algorithm>
#include <print>

namespace org::limitless::fix::utils {

/**
 * Debug helper: prints length bytes from buffer, one per column, with
 * non-printable bytes shown as '?' and SOH (0x01) shown as '|'.
 * @param length number of bytes to print
 * @param buffer bytes to print
 */
inline void print(const uint32_t length, const uint8_t* buffer)
{
    for (uint32_t i = 0; i < length; ++i)
    {
        const auto ch = buffer[i];
        std::printf("%2c ", std::isprint(ch) ? ch : ch == 1 ? '|' : '?');
    }
    std::printf("\n");
}

inline constexpr uint32_t PowersOf10_32[] = {
    1, 10, 100, 1'000, 10'000, 100'000,
    1'000'000, 10'000'000, 100'000'000, 1'000'000'000
};

inline constexpr uint64_t PowersOf10_64[] = {
    1ULL, 10ULL, 100ULL, 1'000ULL, 10'000ULL, 100'000ULL,
    1'000'000ULL, 10'000'000ULL, 100'000'000ULL, 1'000'000'000ULL,
    10'000'000'000ULL, 100'000'000'000ULL, 1'000'000'000'000ULL,
    10'000'000'000'000ULL, 100'000'000'000'000ULL, 1'000'000'000'000'000ULL,
    10'000'000'000'000'000ULL, 100'000'000'000'000'000ULL,
    1'000'000'000'000'000'000ULL, 10'000'000'000'000'000'000ULL
};

/**
 * Multiplies value by 10^power.
 * @param value value to scale
 * @param power power of ten, 0-9; values outside this range return value unchanged
 * @return value * 10^power
 */
[[nodiscard]] constexpr uint32_t scale(const uint32_t value, const uint32_t power)
{
    return power < std::size(PowersOf10_32) ? value * PowersOf10_32[power] : value;
}

/**
 * Multiplies value by 10^power.
 * @param value value to scale
 * @param power power of ten, 0-19; values outside this range return value unchanged
 * @return value * 10^power
 */
[[nodiscard]] constexpr uint64_t scale(const uint64_t value, const uint32_t power)
{
    return power < std::size(PowersOf10_64) ? value * PowersOf10_64[power] : value;
}

inline constexpr int64_t MillisPerDay = 24 * 60 * 60 * 1'000;

inline constexpr uint32_t UTCTimestampShortLength = 17; // "YYYYMMDD-HH:MM:SS"
inline constexpr uint32_t UTCTimestampLength = 21;      // "YYYYMMDD-HH:MM:SS.sss"
inline constexpr uint32_t UTCTimeOnlyShortLength = 8;   // "HH:MM:SS"
inline constexpr uint32_t UTCTimeOnlyLength = 12;       // "HH:MM:SS.sss"
inline constexpr uint32_t UTCDateOnlyLength = 8;        // "YYYYMMDD"

inline constexpr uint64_t SwarMask = 0x000000FF000000FF;
inline constexpr uint64_t SwarFactor1 = 0x000F424000000064; // 100 + (1000000ULL << 32)
inline constexpr uint64_t SwarFactor2 = 0x0000271000000001; // 1 + (10000ULL << 32)

/**
 * Parses up to 8 ASCII digits using SWAR digit parsing and adds the result to
 * value * 10^length.
 * See https://lemire.me/blog/2022/01/21/swar-explained-parsing-eight-digits/
 * @param value accumulator the parsed digits are added to, scaled by 10^length
 * @param digits ASCII digits; must have at least sizeof(uint64_t) readable
 *               bytes, bytes beyond length are shifted out and do not affect
 *               the result
 * @param length number of digits to parse, 0-8
 * @return value * 10^length + the value of the parsed digits
 */
[[nodiscard]] inline uint32_t asciiToUin32(const uint32_t value, const uint8_t* digits, const uint32_t length)
{
    uint64_t number = 0;
    if (length != 0)
    {
        std::memcpy(&number, digits, sizeof(uint64_t));
        number <<= (sizeof(uint64_t) - length) * 8;
    }
    number = number * 10 + (number >> 8); // val = (val * 2561) >> 8;
    number = ((number & SwarMask) * SwarFactor1 + (number >> 16 & SwarMask) * SwarFactor2) >> 32;
    return scale(value, length) + number;
}

inline constexpr uint64_t AsciiZeros = 0x3030303030303030ULL;

/**
 * Parses ASCII digits as an unsigned 64-bit integer, using SWAR digit parsing
 * for the first 8 digits when padded.
 * @tparam CharType 1-byte character type
 * @param digits ASCII digits; if padded, must have at least sizeof(uint64_t)
 *               readable bytes, bytes beyond length are shifted out and do
 *               not affect the result
 * @param length number of digits to parse
 * @param padded true to use the SWAR fast path (digits has at least 8
 *               readable bytes), false to parse digit-by-digit
 * @return parsed value, or 0 if length is 0
 */
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
        std::memcpy(&raw, digits, sizeof(raw));
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

/**
 * Parses ASCII digits as an unsigned 64-bit integer and adds the result to
 * value * 10^length.
 * @tparam T 1-byte character type
 * @param value accumulator the parsed digits are added to, scaled by 10^length
 * @param digits ASCII digits; if padded, must have at least sizeof(uint64_t)
 *               readable bytes, bytes beyond length are shifted out and do
 *               not affect the result
 * @param length number of digits to parse
 * @param padded true to use the SWAR fast path (digits has at least 8
 *               readable bytes), false to parse digit-by-digit
 * @return value * 10^length + the value of the parsed digits
 */
template <typename T>
[[nodiscard]] uint64_t asciiToUint64(const uint64_t value, const T* digits, const uint32_t length, const bool padded)
{
    return scale(value, length) + asciiToUint64(digits, length, padded);
}

/**
 * Parses ASCII digits with an optional leading '-' as a signed 32-bit integer.
 * @param digits ASCII digits, optionally preceded by '-'
 * @param length total length, including the leading '-' if present
 * @return parsed value
 */
[[nodiscard]] inline int32_t asciiToInt32(const uint8_t* digits, const uint32_t length)
{
    if (length != 0 && digits[0] == '-')
    {
        const uint64_t magnitude = asciiToUint64(digits + 1, length - 1, false);
        return static_cast<int32_t>(-static_cast<int64_t>(magnitude));
    }
    return static_cast<int32_t>(asciiToUint64(digits, length, false));
}

/**
 * Wraps a string literal as a span of bytes, excluding the terminating
 * null character.
 * @tparam N size of the string literal, including the null terminator
 * @param str string literal to view
 * @return span over the literal's N - 1 characters
 */
template<size_t N>
[[nodiscard]] constexpr auto makeSpan(const char (& str)[N]) noexcept
{
    return std::span<const uint8_t, N - 1>(reinterpret_cast<const uint8_t*>(str), N - 1);
}

/**
 * SWAR search for a byte value within a little-endian-packed uint64_t.
 * @param value byte value to find
 * @param bytes 8 bytes packed into a uint64_t
 * @return for each matching byte, the high bit of that byte's lane is set;
 *         all other bits are 0
 */
[[nodiscard]] inline uint64_t findByte(const uint8_t value, uint64_t bytes)
{
    const uint64_t mask = 0x01010101'01010101ULL * value;
    bytes ^= mask;
    bytes = (bytes - 0x01010101'01010101ULL) & ~bytes & 0x80808080'80808080ULL;
    return bytes;
}

/**
 * Divides number by a compile-time constant divisor using a multiply-and-shift
 * approximation, avoiding a hardware division instruction.
 * @tparam Divisor compile-time divisor; must be 10, 100, 1'000, or 10'000
 * @param number value to divide
 * @return number / Divisor
 */
template <uint32_t Divisor>
[[nodiscard]] uint32_t fastDivide(const uint32_t number)
{
    static_assert(Divisor == 10 || Divisor == 100 || Divisor == 1'000 || Divisor == 10'000,
                  "fastDivide only supports compile-time divisors of 10 or 100.");
    if constexpr (Divisor == 10)
    {
        return static_cast<uint32_t>(number * 0xCCCCCCCDULL >> 35);
    }
    else if constexpr (Divisor == 100)
    {
        return static_cast<uint32_t>(number * 0x51EB851FULL >> 37);
    }
    else if constexpr (Divisor == 1000)
    {
        return static_cast<uint32_t>((number * 0x20C49BA6ULL) >> 39);
    }
    else if constexpr (Divisor == 10000)
    {
        return static_cast<uint32_t>(number * 0xD1B71759ULL >> 45);
    }
}

/**
 * Converts a Gregorian calendar date to a day count relative to the Unix
 * epoch (1970-01-01), using Howard Hinnant's days-from-civil algorithm.
 * @param year calendar year, e.g. 2026
 * @param month calendar month, 1-12
 * @param day day of month, 1-31
 * @return days since 1970-01-01 (negative for earlier dates)
 */
[[nodiscard]] inline int64_t daysSince1970(int year, int month, int day) noexcept
{
    // Shift the calendar so that March is the first month of the "built-in" year.
    // This trick moves the leap day (Feb 29) to the very end of the calculation loop,
    // making the leap year distribution perfectly linear without if/else blocks.
    year -= (month <= 2) ? 1 : 0;

    const int64_t era = fastDivide<100>(year >= 0 ? year : year - 399) >> 2;
    const int64_t yoe = static_cast<uint32_t>(year - era * 400);            // Year of era
    const int64_t doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1; // Day of year
    const int64_t doe = yoe * 365 + yoe / 4 - fastDivide<100>(yoe) + doy;              // Day of era
    return era * 146097 + doe - 719468;
}

/**
 * Parses a FIX UTCTimestamp ("YYYYMMDD-HH:MM:SS" or "YYYYMMDD-HH:MM:SS.sss")
 * into milliseconds since the Unix epoch.
 * @param data UTCTimestamp bytes; must have at least sizeof(uint64_t)
 *             readable bytes beyond the first 8 (for the SWAR date parse)
 * @param length 17 (no milliseconds) or 21 (with milliseconds)
 * @return milliseconds since the Unix epoch, or -1 if length is neither 17 nor 21
 */
inline int64_t dateTimeToEpochUTC(const uint8_t* data, const uint32_t length)
{
    if (length < UTCTimestampShortLength)
    {
        return -1;
    }
    const uint64_t date = asciiToUint64(data, 8, true);
    const auto years = fastDivide<10000>(date);
    const auto month = fastDivide<100>(date - 10000 * fastDivide<10000>(date));
    const auto day = date - 100 * fastDivide<100>(date);
    const auto days = daysSince1970(years, month, day);

    uint64_t time = 0;
    std::memcpy(&time, data + 9, sizeof(time));
    time -= 0x30303a30303a3030ull;
    const auto hours = (time & 0xff) * 10 + ((time >> 8) & 0xff);
    const auto mins  = (time >> 24 & 0xff) * 10 + (time >> 32 & 0xff);
    const auto secs  = (time >> 48 & 0xff) * 10 + (time >> 56);
    uint64_t millis = days * static_cast<uint64_t>(MillisPerDay) + (hours * 3'600 + mins * 60 + secs) * 1000;
    if (length == UTCTimestampShortLength)
    {
        return millis;
    }
    if (length == UTCTimestampLength)
    {
        time = 0;
        std::memcpy(&time, data + UTCTimestampShortLength, 4);
        time -= 0x3030302e;
        millis += (time >> 24) + (time >> 16 & 0xff) * 10 + (time >> 8 & 0xff) * 100;
        return millis;
    }
    return -1;
}

/**
 * Parses a FIX UTCTimeOnly ("HH:MM:SS" or "HH:MM:SS.sss")
 * into milliseconds since midnight.
 * @param data UTCTimeOnly bytes; must have at least sizeof(uint64_t) readable bytes
 * @param length 8 (no milliseconds) or 12 (with milliseconds)
 * @return milliseconds since midnight, or -1 if length is invalid
 */
inline int64_t timeOnlyToMillis(const uint8_t* data, const uint32_t length)
{
    if (length < UTCTimeOnlyShortLength)
    {
        return -1;
    }
    uint64_t time = 0;
    std::memcpy(&time, data, sizeof(time));
    time -= 0x30303a30303a3030ull;
    const auto hours = (time & 0xff) * 10 + ((time >> 8) & 0xff);
    const auto mins  = (time >> 24 & 0xff) * 10 + (time >> 32 & 0xff);
    const auto secs  = (time >> 48 & 0xff) * 10 + (time >> 56);
    int64_t millis = (hours * 3'600 + mins * 60 + secs) * 1000;
    if (length == UTCTimeOnlyShortLength)
    {
        return millis;
    }
    if (length == UTCTimeOnlyLength)
    {
        time = 0;
        std::memcpy(&time, data + UTCTimeOnlyShortLength, 4);
        time -= 0x3030302e;
        millis += (time >> 24) + (time >> 16 & 0xff) * 10 + (time >> 8 & 0xff) * 100;
        return millis;
    }
    return -1;
}

/**
 * Parses a FIX UTCDateOnly ("YYYYMMDD") into milliseconds since the Unix epoch (midnight UTC).
 * @param data UTCDateOnly bytes; must have at least sizeof(uint64_t) readable bytes
 * @param length must be 8
 * @return milliseconds since the Unix epoch at midnight UTC, or -1 if length is invalid
 */
inline int64_t dateOnlyToEpochUTC(const uint8_t* data, const uint32_t length)
{
    if (length != UTCDateOnlyLength)
    {
        return -1;
    }
    const uint64_t date = asciiToUint64(data, 8, true);
    const auto years = fastDivide<10000>(date);
    const auto month = fastDivide<100>(date - 10000 * fastDivide<10000>(date));
    const auto day = date - 100 * fastDivide<100>(date);
    return daysSince1970(years, month, day) * static_cast<uint64_t>(MillisPerDay);
}

/**
 * Writes the decimal ASCII representation of value (no leading zeros, "0" for
 * zero) into data starting at offset.
 * @param value value to convert
 * @param data destination buffer
 * @param offset byte offset within data at which to write
 * @return number of digits written
 */
inline size_t uint32ToAscii(const uint32_t value, std::span<uint8_t> data, const size_t offset)
{
    if (value == 0)
    {
        data[offset] = '0';
        return 1;
    }

    uint8_t buffer[10];
    if (value < 100000)
    {
        const uint32_t d4 = fastDivide<10000>(value);
        uint32_t reminder = value - d4 * 10000;
        const uint32_t d3 = fastDivide<1000>(reminder);
        reminder = reminder - d3 * 1000;
        const uint32_t d2 = fastDivide<100>(reminder);
        reminder = reminder - d2 * 100;
        const uint32_t d1 = fastDivide<10>(reminder);
        const uint32_t d0 = reminder - d1 * 10;

        uint64_t packedDigits = static_cast<uint64_t>(d0) << 32 |
                                static_cast<uint64_t>(d1) << 24 |
                                static_cast<uint64_t>(d2) << 16 |
                                static_cast<uint64_t>(d3) << 8  |
                                static_cast<uint64_t>(d4) |
                                0xFFFFFF0000000000ULL; // Sentinel high bits
        packedDigits |= 0x0000003030303030ULL;
        size_t skippedZeros = std::countr_zero(packedDigits ^ 0x0000003030303030ULL) >> 3;
        if (skippedZeros > 4) [[unlikely]]
        {
            skippedZeros = 4;
        }
        packedDigits >>= skippedZeros << 3;
        const size_t length = 5 - skippedZeros;
        std::memcpy(data.data() + offset, &packedDigits, length);
        return length;
    }

    uint32_t temp = value;
    size_t index = 10;
    while (temp > 0)
    {
        uint32_t q = fastDivide<10>(temp);
        buffer[--index] = static_cast<uint8_t>((temp - (q * 10)) + '0');
        temp = q;
    }
    const size_t length = 10 - index;
    std::memcpy(data.data() + offset, &buffer[index], length);
    return length;
}

/**
 * Writes the decimal ASCII representation of value into data starting at
 * offset, with a leading '-' for negative values.
 * @param value value to convert
 * @param data destination buffer
 * @param offset byte offset within data at which to write
 * @return number of bytes written, including the leading '-' if present
 */
inline size_t int32ToAscii(const int32_t value, std::span<uint8_t> data, const size_t offset)
{
    if (value < 0)
    {
        data[offset] = '-';
        const uint32_t magnitude = static_cast<uint32_t>(-static_cast<int64_t>(value));
        return 1 + uint32ToAscii(magnitude, data, offset + 1);
    }
    return uint32ToAscii(static_cast<uint32_t>(value), data, offset);
}

/**
 * Writes the decimal ASCII representation of value (no leading zeros, "0" for
 * zero) into data starting at offset.
 * @param value value to convert
 * @param data destination buffer
 * @param offset byte offset within data at which to write
 * @return number of digits written
 */
inline size_t uint64ToAscii(const uint64_t value, std::span<uint8_t> data, const size_t offset)
{
    if (value <= UINT32_MAX)
    {
        return uint32ToAscii(static_cast<uint32_t>(value), data, offset);
    }

    // Always materialize all 20 decimal digits, no data-dependent early exit.
    uint8_t digits[24];
    uint64_t temp = value;
    for (int i = 19; i >= 0; --i)
    {
        digits[i] = static_cast<uint8_t>('0' + (temp % 10));
        temp /= 10;
    }
    digits[20] = digits[21] = digits[22] = digits[23] = '1'; // non-zero sentinel

    uint64_t chunk0, chunk1, chunk2;
    std::memcpy(&chunk0, digits + 0, 8);
    std::memcpy(&chunk1, digits + 8, 8);
    std::memcpy(&chunk2, digits + 16, 8); // digits[16..19] + sentinel[20..23]

    constexpr uint64_t Mask8 = 0x3030303030303030ULL;
    const auto leadingZeroBytes = [](const uint64_t chunk)
    {
        return static_cast<size_t>(std::countl_zero(std::byteswap(chunk ^ Mask8)) >> 3);
    };

    const size_t skip0 = leadingZeroBytes(chunk0);
    const size_t skip1 = leadingZeroBytes(chunk1);
    const size_t skip2 = leadingZeroBytes(chunk2);

    size_t skipped = skip0 < 8 ? skip0 : 8 + (skip1 < 8 ? skip1 : 8 + skip2);
    skipped = std::min(skipped, size_t{19}); // keep at least one digit for value == 0


    const size_t length = 20 - skipped;
    std::memcpy(data.data() + offset, digits + skipped, length);
    return length;
}

/**
 * Writes the decimal ASCII representation of value into data starting at
 * offset, with a leading '-' for negative values.
 * @param value value to convert
 * @param data destination buffer
 * @param offset byte offset within data at which to write
 * @return number of bytes written, including the leading '-' if present
 */
inline size_t int64ToAscii(const int64_t value, std::span<uint8_t> data, const size_t offset)
{
    if (value < 0)
    {
        data[offset] = '-';
        const uint64_t magnitude = uint64_t{0} - static_cast<uint64_t>(value);
        return 1 + uint64ToAscii(magnitude, data, offset + 1);
    }
    return uint64ToAscii(static_cast<uint64_t>(value), data, offset);
}

/**
 * Writes the decimal ASCII representation of mantissa * 10^exponent into data
 * starting at offset, with a leading '-' for negative mantissas and a '.'
 * inserted according to exponent (e.g. mantissa=12345, exponent=-2 writes
 * "123.45"; mantissa=1, exponent=-3 writes "0.001"; mantissa=123, exponent=2
 * writes "12300").
 * @param mantissa signed mantissa
 * @param exponent power-of-ten exponent, <= 0 inserts a decimal point,
 *                  > 0 appends trailing zeros
 * @param data destination buffer
 * @param offset byte offset within data at which to write
 * @return number of bytes written
 */
inline size_t fixedDecimalToAscii(const int64_t mantissa,
                                  const int32_t exponent,
                                  std::span<uint8_t> data,
                                  const size_t offset)
{
    size_t pos = offset;
    uint64_t magnitude = static_cast<uint64_t>(mantissa);
    if (mantissa < 0)
    {
        data[pos++] = '-';
        magnitude = uint64_t{0} - static_cast<uint64_t>(mantissa);
    }
    if (magnitude == 0)
    {
        data[pos++] = '0';
        return pos - offset;
    }

    uint8_t digits[20];
    const size_t digitCount = uint64ToAscii(magnitude, digits, 0);

    if (exponent >= 0)
    {
        std::memcpy(data.data() + pos, digits, digitCount);
        pos += digitCount;
        std::memset(data.data() + pos, '0', static_cast<size_t>(exponent));
        pos += static_cast<size_t>(exponent);
        return pos - offset;
    }

    const size_t fracDigits = static_cast<size_t>(-exponent);
    if (digitCount > fracDigits)
    {
        const size_t intDigits = digitCount - fracDigits;
        std::memcpy(data.data() + pos, digits, intDigits);
        pos += intDigits;
        data[pos++] = '.';
        std::memcpy(data.data() + pos, digits + intDigits, fracDigits);
        pos += fracDigits;
    }
    else
    {
        data[pos++] = '0';
        data[pos++] = '.';
        const size_t leadingZeros = fracDigits - digitCount;
        std::memset(data.data() + pos, '0', leadingZeros);
        pos += leadingZeros;
        std::memcpy(data.data() + pos, digits, digitCount);
        pos += digitCount;
    }
    return pos - offset;
}

/**
 * Writes value as exactly Width decimal digits, zero-padded, e.g.
 * writeFixedDigits<4>(7, dst) writes "0007".
 * @tparam Width number of digits to write
 * @param value value to convert; truncated to its low Width decimal digits
 * @param dst destination buffer, must have Width writable bytes
 */
template <int Width>
void writeFixedDigits(uint32_t value, uint8_t* dst)
{
    for (int i = Width - 1; i >= 0; --i)
    {
        const uint32_t next = fastDivide<10>(value);
        dst[i] = static_cast<uint8_t>('0' + (value - next * 10));
        value = next;
    }
}

/**
 * Writes "YYYYMMDD-" (9 bytes) for the given day number (days since 1970-01-01),
 * the inverse of daysSince1970.
 * @param days days since 1970-01-01
 * @param dst destination buffer, must have 9 writable bytes
 */
inline void writeDatePrefix(const int64_t days, uint8_t* dst)
{
    const int64_t z = days + 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const uint64_t doe = static_cast<uint64_t>(z - era * 146097);
    const uint64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const uint64_t mp = (5 * doy + 2) / 153;
    const uint64_t day = doy - (153 * mp + 2) / 5 + 1;
    const uint64_t month = mp + (mp < 10 ? 3 : -9);
    const int64_t year = static_cast<int64_t>(yoe) + era * 400 + (month <= 2 ? 1 : 0);

    writeFixedDigits<4>(static_cast<uint32_t>(year), dst);
    writeFixedDigits<2>(static_cast<uint32_t>(month), dst + 4);
    writeFixedDigits<2>(static_cast<uint32_t>(day), dst + 6);
    dst[8] = '-';
}

/**
 * Writes "HH:MM:SS.sss" (12 bytes) for the given time of day in milliseconds.
 * @param msOfDay milliseconds since midnight, 0-86'399'999
 * @param dst destination buffer, must have 12 writable bytes
 */
inline void writeTimeOfDay(const uint32_t msOfDay, uint8_t* dst)
{
    writeFixedDigits<2>(msOfDay / 3'600'000, dst);
    dst[2] = ':';
    writeFixedDigits<2>(msOfDay / 60'000 % 60, dst + 3);
    dst[5] = ':';
    writeFixedDigits<2>(msOfDay / 1'000 % 60, dst + 6);
    dst[8] = '.';
    writeFixedDigits<3>(msOfDay % 1'000, dst + 9);
}

/**
 * Writes "YYYYMMDD" (8 bytes) for the given date.
 * @param days days since 1970-01-01
 * @param dst destination buffer, must have 8 writable bytes
 */
inline void writeDateOnly(const int64_t days, uint8_t* dst)
{
    const int64_t z = days + 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const uint64_t doe = static_cast<uint64_t>(z - era * 146097);
    const uint64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const uint64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const uint64_t mp = (5 * doy + 2) / 153;
    const uint64_t day = doy - (153 * mp + 2) / 5 + 1;
    const uint64_t month = mp + (mp < 10 ? 3 : -9);
    const int64_t year = static_cast<int64_t>(yoe) + era * 400 + (month <= 2 ? 1 : 0);

    writeFixedDigits<4>(static_cast<uint32_t>(year), dst);
    writeFixedDigits<2>(static_cast<uint32_t>(month), dst + 4);
    writeFixedDigits<2>(static_cast<uint32_t>(day), dst + 6);
}

/**
 * Parses a FIX UTCTimestamp ("YYYYMMDD-HH:MM:SS" or "YYYYMMDD-HH:MM:SS.sss").
 * @param dateTime UTCTimestamp string
 * @return time since the Unix epoch, or -1ms if dateTime has an unsupported length
 */
inline std::chrono::milliseconds dateTimeToEpochUTC(const std::string_view dateTime)
{
    const auto data = reinterpret_cast<const uint8_t*>(dateTime.data());
    return std::chrono::milliseconds{dateTimeToEpochUTC(data, dateTime.length())};
}

/**
 * Maps a FIX field's string code to the corresponding enum value via
 * Enum::Codes.
 * @tparam Enum enum wrapper type exposing a Codes array and a Values enum,
 *              including Values::Null
 * @param code string code to look up
 * @return the matching enum value, or Enum::Values::Null if code is not found
 */
template <typename Enum>
Enum::Values find(const std::string_view code)
{
    const auto end = Enum::Codes + std::size(Enum::Codes);
    const auto found = std::find(Enum::Codes, end, code);
    if (found != end)
    {
        auto index = std::distance(Enum::Codes, found);
        return static_cast<Enum::Values>(index);
    }
    return Enum::Values::Null;
}

}

#endif //SIMD_FIX_UTILS_HPP
