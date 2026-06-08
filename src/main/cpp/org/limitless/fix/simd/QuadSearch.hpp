//
// Created by Fredrik Dahlberg on 2026-05-09.
//

#ifndef SIMD_FIX_QUADSEARCH_HPP
#define SIMD_FIX_QUADSEARCH_HPP

#include <arm_neon.h>
#include <bit>

namespace org::limitless::fix::simd {
//
// https://lemire.me/blog/2026/04/27/you-can-beat-the-binary-search/
//
// modified to return position instead of presence
//
[[nodiscard]] inline int32_t quadSearch(const uint16_t* carr, const int32_t cardinality, const uint16_t value)
{
    constexpr int32_t Gap = 16;
    if (cardinality < Gap)
    {
        for (int32_t j = 0; j < cardinality; j++)
        {
            if (carr[j] == value)
            {
                return j;
            }
        }
        return -1;
    }

    const int32_t blocks = cardinality / Gap;
    int32_t n = blocks;
    int32_t base = 0;
    while (n > 3)
    {
        const int32_t quarter = n >> 2;
        const int32_t k1 = carr[(base + quarter + 1) * Gap - 1];
        const int32_t k2 = carr[(base + 2 * quarter + 1) * Gap - 1];
        const int32_t k3 = carr[(base + 3 * quarter + 1) * Gap - 1];
        const int32_t c1 = k1 < value;
        const int32_t c2 = k2 < value;
        const int32_t c3 = k3 < value;
        base += (c1 + c2 + c3) * quarter;
        n -= 3 * quarter;
    }
    while (n > 1)
    {
        const int32_t half = n >> 1;
        base = carr[(base + half + 1) * Gap - 1] < value ? base + half : base;
        n -= half;
    }

    const int32_t lo = carr[(base + 1) * Gap - 1] < value ? base + 1 : base;
    if (lo < blocks)
    {
        const uint16_t* blk = carr + lo * Gap;
        const uint16x8_t needle = vdupq_n_u16(value);
        const uint16x8_t m0 = vceqq_u16(vld1q_u16(blk), needle);
        const uint16x8_t m1 = vceqq_u16(vld1q_u16(blk + 8), needle);
        const uint8x8_t n0 = vshrn_n_u16(m0, 4);
        const uint8x8_t n1 = vshrn_n_u16(m1, 4);
        const uint64_t low  = vget_lane_u64(vreinterpret_u64_u8(n0), 0);
        const uint64_t high = vget_lane_u64(vreinterpret_u64_u8(n1), 0);
        const uint64_t combined = low | high;
        return combined != 0 ? (std::countr_zero(combined) >> 3) + ((high != 0) << 3) : -1;
    }
    for (int32_t j = blocks * Gap; j < cardinality; j++)
    {
        if (carr[j] >= value)
        {
            return j;
        }
    }
    return -1;
}
}

#endif //SIMD_FIX_QUADSEARCH_HPP
