//
// Created by Fredrik Dahlberg on 2026-05-16.
//

#ifndef SIMDFIX_SIMDLINEAR_HPP
#define SIMDFIX_SIMDLINEAR_HPP

#include <arm_neon.h>
#include <bit>

namespace org::limitless::fix::simd {
[[nodiscard]] inline int32_t find(const uint16_t* values, const int32_t cardinality, const uint16_t value)
{
    const uint16x8_t vkey = vdupq_n_u16(value);

    int32_t i = 0;
    for (; i <= cardinality - 16; i += 16)
    {
        const uint16x8_t m0 = vceqq_u16(vld1q_u16(values + i), vkey);
        const uint16x8_t m1 = vceqq_u16(vld1q_u16(values + i + 8), vkey);
        const uint8x8_t n0 = vshrn_n_u16(m0, 4);
        const uint8x8_t n1 = vshrn_n_u16(m1, 4);
        const uint64_t low  = vget_lane_u64(vreinterpret_u64_u8(n0), 0);
        const uint64_t high = vget_lane_u64(vreinterpret_u64_u8(n1), 0);
        const uint64_t combined = low | high;
        if (combined != 0)
        {
            return i + (std::countr_zero(combined) >> 3) + ((high != 0) << 3);
        }
    }
    for (; i < cardinality; ++i)
    {
        if (values[i] == value) return i;
    }
    return -1;
}
}

#endif //SIMDFIX_SIMDLINEAR_HPP
