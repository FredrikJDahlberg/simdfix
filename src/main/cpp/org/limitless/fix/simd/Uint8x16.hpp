//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#ifndef SIMD_UNT8X16_H
#define SIMD_UNT8X16_H

#include <concepts>
#include <print>

#include <arm_neon.h>

namespace org::limitless::fix::simd {

struct Uint8x16
{
    typedef uint8x16_t value_type;

    static uint32_t constexpr Size = 16;

    static inline const value_type Ones = vdupq_n_u8(255);
    static inline const value_type Zeros = vdupq_n_u8(0);

    static inline const value_type AsciiZeros = vdupq_n_u8('0');
    static inline const value_type AsciiNines = vdupq_n_u8('9');

    value_type m_block{};

    /**
     * Creates a zero-filled vector.
     */
    Uint8x16() : Uint8x16{0}
    {
    }

    /**
     * Copies the block of another vector.
     * @param block vector to copy
     */
    explicit Uint8x16(const Uint8x16& block)
    {
        m_block = block.m_block;
    }

    /**
     * Wraps an existing NEON 128-bit register value.
     * @param block 16 byte vector
     */
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
     * vdupq_n_u16 is a NEON SIMD intrinsic used to broadcast a single
     * 16-bit scalar across all 8 lanes of a 128-bit vector. Constrained to
     * exact uint16_t arguments so int/char literals keep selecting the
     * byte-broadcast constructor.
     * @param filler 16-bit value
     */
    explicit Uint8x16(const std::same_as<uint16_t> auto filler)
    {
        m_block = vreinterpretq_u8_u16(vdupq_n_u16(filler));
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

    /**
     * vld1q_u8 is a NEON SIMD intrinsic that loads 16 bytes (128 bits) of
     * unsigned 8-bit integers from memory into a single register. Shorter
     * buffers are copied into a zero-filled temporary first, so the
     * trailing lanes read as zero.
     * @param buffer data
     * @param length valid bytes
     * @return this vector
     */
    Uint8x16& put(const uint8_t* buffer, const size_t length)
    {
        if (length >= 16) [[likely]]
        {
            m_block = vld1q_u8(buffer);
        }
        else [[unlikely]]
        {
            alignas(16) uint8_t aligned[16] = {0};
            for (size_t i = 0; i < length; ++i)
            {
                aligned[i] = buffer[i];
            }
            m_block = vld1q_u8(aligned);
        }
        return *this;
    }

    /**
     * vld1q_u16 is a NEON SIMD intrinsic that loads 8 unsigned 16-bit
     * integers (128 bits) from memory into a single register. Shorter
     * buffers are copied into a zero-filled temporary first, so the
     * trailing lanes read as zero.
     * @param buffer data
     * @param length valid bytes
     * @return this vector
     */
    Uint8x16& put(const uint16_t* buffer, const size_t length)
    {
        if (length >= 16) [[likely]]
        {
            m_block = vreinterpretq_u8_u16(vld1q_u16(buffer));
        }
        else [[unlikely]]
        {
            alignas(16) uint16_t temp[8] = {0};
            for (size_t i = 0; i < length / sizeof(uint16_t); ++i)
            {
                temp[i] = buffer[i];
            }
            m_block = vreinterpretq_u8_u16(vld1q_u16(temp));
        }
        return *this;
    }

    /**
     * vld1q_u8 loads 16 bytes from memory without a length check.
     * @param buffer data, at least 16 valid bytes
     */
    void load(const uint8_t* buffer)
    {
        m_block = vld1q_u8(buffer);
    }

    /**
     * vst1q_u8 is a NEON SIMD intrinsic that stores 16 bytes (128 bits)
     * of the vector to memory.
     * @param position offset into buffer
     * @param buffer destination, at least position + 16 bytes
     * @return this vector
     */
    const Uint8x16& get(const size_t position, uint8_t* buffer) const
    {
        vst1q_u8(buffer + position, m_block);
        return *this;
    }

    /**
     * vorrq_u8 computes the bitwise OR of two vectors, in place.
     * @param block vector to OR with
     * @return this vector
     */
    Uint8x16& operator|=(const Uint8x16& block)
    {
        m_block = vorrq_u8(m_block, block.m_block);
        return *this;
    }

    /**
     * vorrq_u8 computes the bitwise OR of two vectors.
     * @param block vector to OR with
     * @return OR of the vectors
     */
    Uint8x16 operator|(const Uint8x16& block) const
    {
        return Uint8x16{vorrq_u8(this->m_block, block.m_block)};
    }

    /**
     * vandq_u8 computes the bitwise AND of two vectors, in place.
     * @param block vector to AND with
     * @return this vector
     */
    Uint8x16& operator&=(const Uint8x16& block)
    {
        m_block = vandq_u8(m_block, block.m_block);
        return *this;
    }

    /**
     * vandq_u8 computes the bitwise AND of two vectors.
     * @param block vector to AND with
     * @return AND of the vectors
     */
    Uint8x16 operator&(const Uint8x16& block) const
    {
        return Uint8x16{vandq_u8(this->m_block, block.m_block)};
    }

    /**
     * vextq_u8 extracts a 16-byte window from a vector pair, used here to
     * shift the lanes N positions towards lower indices, filling the upper
     * lanes with zeros.
     * @tparam N positions to shift, 0..15
     * @return shifted vector
     */
    template <int N>
    [[nodiscard]] Uint8x16 shiftLeft() const {
        static_assert(N >= 0 && N < 16, "Shift must be between 0 and 15");
        return Uint8x16(vextq_u8(m_block, Zeros, N));
    }

    /**
     * vextq_u8 extracts a 16-byte window from a vector pair, used here to
     * shift the lanes N positions towards higher indices, filling the lower
     * lanes with zeros.
     * @tparam N positions to shift, 0..15
     * @return shifted vector
     */
    template <int N>
    [[nodiscard]] Uint8x16 shiftRight() const
    {
        static_assert(N >= 0 && N < 16, "Shift must be between 0 and 15");
        return Uint8x16(vextq_u8(Zeros, m_block, 16 - N));
    }

    /**
     * vsubq_u8 subtracts the lanes byte-wise, wrapping modulo 256.
     * @param block subtrahend
     * @return difference vector
     */
    Uint8x16 operator-(const Uint8x16& block) const
    {
        return Uint8x16(vsubq_u8(m_block, block.m_block));
    }

    /**
     * vsubq_u8 subtracts the lanes byte-wise in place, wrapping modulo 256.
     * @param block subtrahend
     * @return this vector
     */
    Uint8x16& operator-=(const Uint8x16& block)
    {
        m_block = vsubq_u8(m_block, block.m_block);
        return *this;
    }

    /**
     * vaddq_u8 adds the lanes byte-wise, wrapping modulo 256.
     * @param block addend
     * @return sum vector
     */
    Uint8x16 operator+(const Uint8x16& block) const
    {
        return Uint8x16(vaddq_u8(m_block, block.m_block));
    }

    /**
     * vaddq_u8 adds the lanes byte-wise in place, wrapping modulo 256.
     * @param block addend
     * @return this vector
     */
    Uint8x16& operator+=(const Uint8x16& block)
    {
        m_block = vaddq_u8(m_block, block.m_block);
        return *this;
    }

    /**
     * Assigns a raw NEON register value to the vector.
     * @param vector 16 byte vector
     * @return this vector
     */
    Uint8x16& operator=(const value_type& vector)
    {
        m_block = vector;
        return *this;
    }

    /**
     * Copies the block of another vector.
     * @param block vector to copy
     * @return this vector
     */
    Uint8x16& operator=(const Uint8x16& block)
    {
        if (&block != this)
        {
            m_block = block.m_block;
        }
        return *this;
    }

    /**
     * vceqq_u8 compares the lanes byte-wise for equality.
     * @param block vector to compare with
     * @return mask vector, 0xFF per matching lane
     */
    Uint8x16 operator==(const Uint8x16& block) const
    {
        return Uint8x16{vceqq_u8(this->m_block, block.m_block)};
    }

    /**
     * vcgeq_u8 compares the lanes byte-wise for greater-or-equal.
     * @param block vector to compare with
     * @return mask vector, 0xFF per lane where this >= block
     */
    Uint8x16 operator>=(const Uint8x16& block) const
    {
        return Uint8x16{vcgeq_u8(m_block, block.m_block)};
    }

    /**
     * vcleq_u8 compares the lanes byte-wise for less-or-equal.
     * @param block vector to compare with
     * @return mask vector, 0xFF per lane where this <= block
     */
    Uint8x16 operator<=(const Uint8x16& block) const
    {
        return Uint8x16{vcleq_u8(m_block, block.m_block)};
    }

    /**
     * vaddlvq_u8 is a NEON SIMD intrinsic that adds all 16 lanes into a
     * single widened sum.
     * @return sum of all lanes, 0..4080
     */
    [[nodiscard]] uint64_t sum() const
    {
        return vaddlvq_u8(m_block);
    }

    /**
     * @return the underlying NEON register value
     */
    [[nodiscard]] value_type data() const
    {
        return m_block;
    }

    /**
     * vbslq_u8 is a NEON SIMD intrinsic performing a bitwise select; this
     * vector acts as the mask, selecting bits from block where set and
     * zero elsewhere.
     * @param block values to pass through where the mask is set
     * @return selected vector
     */
    [[nodiscard]] Uint8x16 whenTrue(const Uint8x16& block) const
    {
    return Uint8x16{vbslq_u8(m_block, block.m_block, Zeros)};
    }

    /**
     * vceqq_u8 compares the lanes byte-wise for equality, writing the mask
     * to an out parameter so the source vector can be chained further.
     * @param block vector to compare with
     * @param result mask vector, 0xFF per matching lane
     * @return this vector
     */
    Uint8x16& equal(const Uint8x16& block, Uint8x16& result)
    {
        result.m_block = vceqq_u8(m_block, block.m_block);
        return *this;
    }

    /**
     * Lane-wise 16-bit equality (vceqq_u16); a matching lane is all ones.
     * Byte-wise operator== cannot be used for 16-bit keys since a lane
     * matching in only one of its two bytes must not count as a match.
     * @param block 16-bit lanes to compare with
     * @return mask vector, 0xFFFF per matching 16-bit lane
     */
    [[nodiscard]] Uint8x16 equal(const Uint8x16& block) const
    {
        return Uint8x16{vreinterpretq_u8_u16(
            vceqq_u16(vreinterpretq_u16_u8(m_block), vreinterpretq_u16_u8(block.m_block)))};
    }

    /**
     * Compresses the vector into a 64-bit mask with 4 bits per byte lane:
     * vtstq_u8 maps nonzero lanes to all ones, then vshrn_n_u16 narrows
     * each 16-bit pair to one byte, leaving a nibble per original lane.
     * @return 64-bit mask, 0xF per nonzero lane
     */
    [[nodiscard]] uint64_t toUint64() const
    {
        const uint8x16_t mask = vtstq_u8(m_block, m_block);
        const uint8x8_t narrowed = vshrn_n_u16(vreinterpretq_u16_u8(mask), 4);
        return vget_lane_u64(vreinterpret_u64_u8(narrowed), 0);
    }

    /**
     * Prints the lanes as space-separated hex bytes followed by a newline.
     */
    void print() const
    {
        print(m_block);
    }

    /**
     * Prints the lanes of a raw NEON register as space-separated hex bytes
     * followed by a newline.
     * @param block 16 byte vector
     */
    static void print(const uint8x16_t& block)
    {
        const auto data = reinterpret_cast<const uint8_t*>(&block);
        for (int i = 0; i < 16; ++i)
        {
            std::print("{:02x} ", data[i]);
        }
        std::println();
    }
};

}

#endif // SIMD_UNT8X16_H
