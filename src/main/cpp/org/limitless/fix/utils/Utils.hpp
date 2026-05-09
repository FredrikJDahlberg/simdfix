//
// Created by Fredrik Dahlberg on 2026-04-23.
//

#ifndef SIMD_FIX_UTILS_HPP
#define SIMD_FIX_UTILS_HPP

#include <arm_neon.h>
#include <arm_vector_types.h>
#include <cstdint>

#include "org/limitless/fix/simd/Uint8x16.hpp"

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

inline uint32_t asciiToDecimal(uint32_t value, const uint8_t* digits, const uint32_t length)
{
    if (length >= 5)
    {
        for (uint32_t position = 0; position < length; ++position)
        {
            value = value * 10 + digits[position] - '0';
        }
    } else
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
    size_t n = str.size() > 8 ? 8 : str.size();
    uint64_t result = 0;
    for (size_t i = 0; i < n; ++i)
    {
        result |= (static_cast<uint64_t>(static_cast<uint8_t>(str[i])) << (i * 8));
    }
    return result;
}

//
// https://lemire.me/blog/2026/04/27/you-can-beat-the-binary-search/
//
// modified to return position
//
inline int32_t simd_quad(const uint16_t* carr, int32_t cardinality, uint16_t pos)
{
    constexpr int32_t gap = 16;
    if (cardinality < gap)
    {
        for (int32_t j = 0; j < cardinality; j++)
        {
            if (carr[j] == pos)
            {
                return j;
            }
        }
        return -1;
    }

    int32_t num_blocks = cardinality / gap;
    int32_t base = 0;
    int32_t n = num_blocks;
    while (n > 3)
    {
        int32_t quarter = n >> 2;
        int32_t k1 = carr[(base + quarter + 1) * gap - 1];
        int32_t k2 = carr[(base + 2 * quarter + 1) * gap - 1];
        int32_t k3 = carr[(base + 3 * quarter + 1) * gap - 1];
        int32_t c1 = (k1 < pos);
        int32_t c2 = (k2 < pos);
        int32_t c3 = (k3 < pos);
        base += (c1 + c2 + c3) * quarter;
        n -= 3 * quarter;
    }
    while (n > 1)
    {
        int32_t half = n >> 1;
        base = (carr[(base + half + 1) * gap - 1] < pos) ? base + half : base;
        n -= half;
    }

    int32_t lo = (carr[(base + 1) * gap - 1] < pos) ? base + 1 : base;
    if (lo < num_blocks)
    {
        const uint16_t* blk = carr + lo * gap;
        const uint16x8_t needle = vdupq_n_u16(pos);
        const uint16x8_t m0 = vceqq_u16(vld1q_u16(blk), needle);
        const uint16x8_t m1 = vceqq_u16(vld1q_u16(blk + 8), needle);
        const uint8x8_t n0 = vshrn_n_u16(m0, 4);
        const uint8x8_t n1 = vshrn_n_u16(m1, 4);
        const uint64_t low  = vget_lane_u64(vreinterpret_u64_u8(n0), 0);
        const uint64_t high = vget_lane_u64(vreinterpret_u64_u8(n1), 0);
        const uint64_t combined = low | high;
        if (combined == 0)
        {
            return -1;
        }
        return (std::countr_zero(combined) >> 3) + ((high != 0) << 3);
    }
    for (int32_t j = num_blocks * gap; j < cardinality; j++)
    {
        if (carr[j] >= pos)
        {
            return j;
        }
    }
    return -1;
}

[[nodiscard]] inline int32_t fastLinearSearch(const uint16_t* array, const size_t count, const uint16_t value)
{
    return simd_quad(array, count, value);
}
}

#endif //SIMD_FIX_UTILS_HPP
