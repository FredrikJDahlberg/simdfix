//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#ifndef SIMD_H
#define SIMD_H

#include <arm_neon.h>
#include <iostream>

namespace org::limitless::simd {

struct Block
{
    typedef uint8x16_t value_type;

    static inline const value_type ZeroFiller = vdupq_n_u8(0);

    static inline const value_type AsciiZeros = vdupq_n_u8('0');
    static inline const value_type AsciiNines = vdupq_n_u8('9');

    value_type m_block{};

    Block() : Block{0}
    {
    }

    explicit Block(const Block& block)
    {
        m_block = block.m_block;
    }

    explicit Block(const value_type block) : m_block(block)
    {
    }

    /**
     * vdupq_n_u8 is a NEON SIMD intrinsic used to broadcast a single
     * scalar byte across all 16 lanes of a 128-bit vector.
     * @param filler byte
     * @return 16 byte vector filled with filler bytes
     */
    explicit Block(const uint8_t filler)
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
    explicit Block(const uint8_t* buffer, const size_t length)
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

    const Block& get(const size_t position, uint8_t* buffer) const
    {
        vst1q_u8(buffer + position, m_block);
        return *this;
    }

    Block& operator|=(const Block& block)
    {
        m_block = vorrq_u8(m_block, block.m_block);
        return *this;
    }

    Block operator|(const Block& block) const
    {
        return Block{vorrq_u8(this->m_block, block.m_block)};
    }

    Block& operator&=(const Block& block)
    {
        m_block = vandq_u8(m_block, block.m_block);
        return *this;
    }

    Block operator&(const Block& block) const
    {
        return Block{vandq_u8(this->m_block, block.m_block)};
    }

    template <int N>
    [[nodiscard]] Block shiftLeft() const {
        static_assert(N >= 0 && N < 16, "Shift must be between 0 and 15");
        return Block(vextq_u8(m_block, ZeroFiller, N));
    }

    template <int N>
    Block& shiftLeft()
    {
        m_block = vextq_u8(m_block, ZeroFiller, N);
        return *this;
    }

    template <int N>
    [[nodiscard]] Block shiftRight() const
    {
        static_assert(N >= 0 && N < 16, "Shift must be between 0 and 15");
        return Block(vextq_u8(ZeroFiller, m_block, 16 - N));
    }

    template <int N>
    Block& shiftRight()
    {
        static_assert(N >= 0 && N < 16, "Shift must be between 0 and 15");
        m_block = vextq_u8(ZeroFiller, m_block, 16 - N);
        return *this;
    }

    Block operator-(const Block& block) const
    {
        return Block(vsubq_u8(m_block, block.m_block));
    }

    Block& operator-=(const Block& block)
    {
        m_block = vsubq_u8(m_block, block.m_block);
        return *this;
    }

    // TODO +=

    Block& operator=(const value_type& vector)
    {
        m_block = vector;
        return *this;
    }

    Block& operator=(const Block& block)
    {
        if (&block != this)
        {
            m_block = block.m_block;
        }
        return *this;
    }

    Block operator==(const Block& block) const
    {
        return Block{vceqq_u8(this->m_block, block.m_block)};
    }

    Block operator>=(const Block& block) const
    {
        return Block{vcgeq_u8(m_block, block.m_block)};
    }

    Block operator<=(const Block& block) const
    {
        return Block{vcleq_u8(m_block, block.m_block)};
    }

    [[nodiscard]] Block ifElse(const Block& trueBlock, const Block& falseBlock) const
    {
        return Block{vbslq_u8(m_block, trueBlock.m_block, falseBlock.m_block)};
    }

    Block& equal(const Block& block, Block& result)
    {
        result.m_block = vceqq_u8(m_block, block.m_block);
        return *this;
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
};

}

#endif // SIMD_H