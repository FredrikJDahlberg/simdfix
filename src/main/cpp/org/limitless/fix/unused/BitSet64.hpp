//
// Created by Fredrik Dahlberg on 2026-04-24.
//

#ifndef SIMD_FIX_BITSET_HPP
#define SIMD_FIX_BITSET_HPP

#include <bit>
#include <cstddef>

namespace org::limitless::fix::decoder {

/**
 * Thin wrapper around a 64-bit unsigned integer treated as a fixed-size
 * bitset, with bit-counting helpers used to walk SWAR masks (e.g. the
 * tag/field-end masks produced by findByte()).
 */
class BitSet64
{
public:
    BitSet64() : m_bits(0ull)
    {
    }

    BitSet64(const BitSet64& other) = default;
    BitSet64(BitSet64&& other) noexcept = default;
    BitSet64& operator=(const BitSet64& other) = default;
    BitSet64& operator=(BitSet64&& other) noexcept = default;

    /**
     * @param bits initial bit pattern
     */
    explicit BitSet64(const uint64_t bits) : m_bits(bits)
    {
    }

    /**
     * @param position number of bit positions to shift right
     * @return *this, for chaining
     */
    BitSet64& operator>>=(const size_t position) noexcept
    {
        m_bits >>= position;
        return *this;
    }

    /**
     * @return the number of bits this set can hold (64)
     */
    [[nodiscard]] size_t capacity() const
    {
        return sizeof(m_bits) * 8;
    }

    /**
     * @param position number of bit positions to shift left
     * @return *this, for chaining
     */
    BitSet64& operator<<=(const size_t position) noexcept
    {
        m_bits <<= position;
        return *this;
    }

    /**
     * Sets all bits.
     * @return *this, for chaining
     */
    BitSet64& set() noexcept
    {
        m_bits = static_cast<uint64_t>(-1LL);
        return *this;
    }

    /**
     * Sets the bit at position.
     * @param position bit index, 0-63
     * @return *this, for chaining
     */
    BitSet64& set(const int32_t position)
    {
        m_bits |= 1ULL << position;
        return *this;
    }

    /**
     * @param position bit index, 0-63
     * @return the bit at position, as 0 or 1ULL << position
     */
    [[nodiscard]] uint64_t get(const int32_t position) const noexcept
    {
        return m_bits & 1ULL << position;
    }

    /**
     * Clears all bits.
     * @return *this, for chaining
     */
    BitSet64& clear() noexcept
    {
        m_bits = 0ULL;
        return *this;
    }

    /**
     * Clears the bit at position.
     * @param position bit index, 0-63
     * @return *this, for chaining
     */
    BitSet64& clear(const size_t position) noexcept
    {
        m_bits &= ~(1ULL << position);
        return *this;
    }

    /**
     * @return the number of trailing (low-order) zero bits, or 64 if no bits are set
     */
    [[nodiscard]] int32_t zerosRight() const noexcept
    {
        return std::countr_zero(m_bits);
    }

    /**
     * @return the number of leading (high-order) zero bits, or 64 if no bits are set
     */
    [[nodiscard]] int32_t zerosLeft() const noexcept
    {
        return std::countl_zero(m_bits);
    }

    /**
     * @return the number of trailing (low-order) one bits, or 64 if all bits are set
     */
    [[nodiscard]] int32_t onesRight() const noexcept
    {
        return std::countr_one(m_bits);
    }

    /**
     * @return the number of leading (high-order) one bits, or 64 if all bits are set
     */
    [[nodiscard]] int32_t onesLeft() const noexcept
    {
        return std::countl_one(m_bits);
    }

    /**
     * @return true if no bits are set
     */
    [[nodiscard]] bool empty() const noexcept
    {
        return m_bits == 0;
    }

    /**
     * @return the underlying bit pattern
     */
    [[nodiscard]] uint64_t data() const noexcept
    {
        return  m_bits;
    }

private:
    uint64_t m_bits;
};


}

#endif //SIMD_FIX_BITSET_HPP
