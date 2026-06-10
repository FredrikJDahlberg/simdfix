//
// Created by Fredrik Dahlberg on 2026-05-16.
//

#ifndef SIMD_FIX_LINEAR_HPP
#define SIMD_FIX_LINEAR_HPP

#include <arm_neon.h>
#include <bit>

namespace org::limitless::fix::simd {

[[nodiscard]] inline int32_t find(const uint16_t* values, const int32_t cardinality, const uint16_t value)
{
    const uint16x8_t vkey = vdupq_n_u16(value);
    int32_t i = 0;
    for (; i <= cardinality - 8; i += 8)
    {
        const uint16x8_t m = vceqq_u16(vld1q_u16(values + i), vkey);
        const uint8x8_t n = vshrn_n_u16(m, 4);
        const uint64_t bits = vget_lane_u64(vreinterpret_u64_u8(n), 0);
        if (bits != 0)
        {
            return i + (std::countr_zero(bits) >> 3);
        }
    }
    for (; i < cardinality; ++i)
    {
        if (values[i] == value)
        {
            return i;
        }
    }
    return -1;
}
}

#endif //SIMD_FIX_LINEAR_HPP
