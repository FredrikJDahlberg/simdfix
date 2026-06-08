//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#ifndef SIMD_UNT8X16_H
#define SIMD_UNT8X16_H

#include <iostream>

#include <arm_neon.h>

namespace org::limitless::fix::simd {

struct Uint8x16
{
    typedef uint8x16_t value_type;

    static size_t constexpr Size = 16;

    static inline const value_type Ones = vdupq_n_u8(255);
    static inline const value_type Zeros = vdupq_n_u8(0);

    static inline const value_type AsciiZeros = vdupq_n_u8('0');
    static inline const value_type AsciiNines = vdupq_n_u8('9');

    value_type m_block{};

    Uint8x16() : Uint8x16{0}
    {
    }

    explicit Uint8x16(const Uint8x16& block)
    {
        m_block = block.m_block;
    }

    explicit Uint8x16(const value_type block) : m_block(block)
    {
    }

    /**
     * vdupq_n_u8 is a NEON SIMD intrinsic used to broadcast a single
     * scalar byte across all 16 lanes of a 128-bit vector.
     * @param filler byte
     * @return 16 byte vector filled with filler bytes
     */
    explicit Uint8x16(const uint8_t filler)
    {
        m_block = vdupq_n_u8(filler);
    }

    /**
     * vld1q_u8 is a NEON SIMD intrinsic that loads 16 bytes (128 bits) of
     * unsigned 8-bit integers from memory into a single register.
     * @param buffer data
     * @param length valid data
     * @return 16 byte vector
     */
    explicit Uint8x16(const uint8_t* buffer, const size_t length)
    {
        put(buffer, length);
    }

    // load from memory
    bool put(const uint8_t* buffer, const size_t length)
    {
        const bool success = length >= 16;
        if (success)
        {
            m_block = vld1q_u8(buffer);
        }
        return success;
    }

    void load(const uint8_t* buffer)
    {
        m_block = vld1q_u8(buffer);
    }

    const Uint8x16& get(const size_t position, uint8_t* buffer) const
    {
        vst1q_u8(buffer + position, m_block);
        return *this;
    }

    Uint8x16& operator|=(const Uint8x16& block)
    {
        m_block = vorrq_u8(m_block, block.m_block);
        return *this;
    }

    Uint8x16 operator|(const Uint8x16& block) const
    {
        return Uint8x16{vorrq_u8(this->m_block, block.m_block)};
    }

    Uint8x16& operator&=(const Uint8x16& block)
    {
        m_block = vandq_u8(m_block, block.m_block);
        return *this;
    }

    Uint8x16 operator&(const Uint8x16& block) const
    {
        return Uint8x16{vandq_u8(this->m_block, block.m_block)};
    }

    template <int N>
    [[nodiscard]] Uint8x16 shiftLeft() const {
        static_assert(N >= 0 && N < 16, "Shift must be between 0 and 15");
        return Uint8x16(vextq_u8(m_block, Zeros, N));
    }

    template <int N>
    [[nodiscard]] Uint8x16 shiftRight() const
    {
        static_assert(N >= 0 && N < 16, "Shift must be between 0 and 15");
        return Uint8x16(vextq_u8(Zeros, m_block, 16 - N));
    }

    Uint8x16 operator-(const Uint8x16& block) const
    {
        return Uint8x16(vsubq_u8(m_block, block.m_block));
    }

    Uint8x16& operator-=(const Uint8x16& block)
    {
        m_block = vsubq_u8(m_block, block.m_block);
        return *this;
    }

    // TODO +=

    Uint8x16& operator=(const value_type& vector)
    {
        m_block = vector;
        return *this;
    }

    Uint8x16& operator=(const Uint8x16& block)
    {
        if (&block != this)
        {
            m_block = block.m_block;
        }
        return *this;
    }

    Uint8x16 operator==(const Uint8x16& block) const
    {
        return Uint8x16{vceqq_u8(this->m_block, block.m_block)};
    }

    Uint8x16 operator>=(const Uint8x16& block) const
    {
        return Uint8x16{vcgeq_u8(m_block, block.m_block)};
    }

    Uint8x16 operator<=(const Uint8x16& block) const
    {
        return Uint8x16{vcleq_u8(m_block, block.m_block)};
    }

    [[nodiscard]] uint64_t sum() const
    {
        return vaddlvq_u8(m_block);
    }

    [[nodiscard]] value_type data() const
    {
        return m_block;
    }

    [[nodiscard]] Uint8x16 whenTrue(const Uint8x16& block) const
    {
    return Uint8x16{vbslq_u8(m_block, block.m_block, Zeros)};
    }

    Uint8x16& equal(const Uint8x16& block, Uint8x16& result)
    {
        result.m_block = vceqq_u8(m_block, block.m_block);
        return *this;
    }

    [[nodiscard]] uint64_t toUint64() const
    {
        const uint8x16_t mask = vtstq_u8(m_block, m_block);
        const uint8x8_t narrowed = vshrn_n_u16(vreinterpretq_u16_u8(mask), 4);
        return vget_lane_u64(vreinterpret_u64_u8(narrowed), 0);
    }

    void print() const
    {
        const auto data = reinterpret_cast<const uint8_t*>(&m_block);
        for (int i = 0; i < 16; ++i)
        {
            std::printf("%02x ", data[i]);
        }
        std::printf("\n");
    }

    static void dump(const uint8x16_t& vector)
    {
        const auto data = reinterpret_cast<const uint8_t*>(&vector);
        for (int i = 0; i < 16; ++i)
        {
            std::printf("%02x ", data[i]);
        }
        std::printf("\n");
    }
};

}

#endif // SIMD_UNT8X16_H
