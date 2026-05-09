//
// Created by Fredrik Dahlberg on 2026-05-08.
//

#ifndef SIMD_FIX_UINT16X8_HPP
#define SIMD_FIX_UINT16X8_HPP

#include "Uint8x16.hpp"

#include <arm_neon.h>

namespace org::limitless::simd {

struct Uint16x8
{
    typedef uint16x8_t value_type;

    value_type m_block;

    //uint16x8_t v1 = vld1q_u16(blk + 8);
    //uint16x8_t needle = vdupq_n_u16(pos);
    //uint16x8_t m0 = vceqq_u16(v0, needle);

    Uint16x8() : Uint16x8{0}
    {
    }

    explicit Uint16x8(const Uint16x8& block)
    {
        m_block = block.m_block;
    }

    explicit Uint16x8(const value_type block) : m_block(block)
    {
    }

    explicit Uint16x8(const uint8_t filler)
    {
        m_block = vdupq_n_u16(filler);
    }

    Uint16x8 operator==(const Uint16x8& block) const
    {
        return Uint16x8{vceqq_u16(this->m_block, block.m_block)};
    }

    Uint8x16 toUint8x16() const
    {
        uint8x8_t n0 = vshrn_n_u16(m0.data(), 4);
        uint8x8_t n1 = vshrn_n_u16(m1.data(), 4);
        return Uint8x16{vcombine_u8(n0, n1)};
    }
};
}

#endif //SIMD_FIX_UINT16X8_HPP
