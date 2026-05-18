//
// Created by Fredrik Dahlberg on 2026-04-24.
//

#ifndef SIMD_FIX_BITSET_HPP
#define SIMD_FIX_BITSET_HPP

#include <bit>

namespace org::limitless::fix::decoder {

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

    explicit BitSet64(const uint64_t bits) : m_bits(bits)
    {
    }

    BitSet64& operator>>=(const size_t position) noexcept
    {
        m_bits >>= position;
        return *this;
    }

    [[nodiscard]] size_t capacity() const
    {
        return sizeof(m_bits) * 8;
    }

    BitSet64& operator<<=(const size_t position) noexcept
    {
        m_bits <<= position;
        return *this;
    }

    BitSet64& set() noexcept
    {
        m_bits = static_cast<uint64_t>(-1LL);
        return *this;
    }

    BitSet64& set(const int32_t position)
    {
        m_bits |= 1ULL << position;
        return *this;
    }

    [[nodiscard]] uint64_t get(const int32_t position) const noexcept
    {
        return m_bits & 1ULL << position;
    }

    BitSet64& clear() noexcept
    {
        m_bits = 0ULL;
        return *this;
    }

    BitSet64& clear(const size_t position) noexcept
    {
        m_bits &= ~(1ULL << position);
        return *this;
    }

    [[nodiscard]] int32_t zerosRight() const noexcept
    {
        return std::countr_zero(m_bits);
    }

    [[nodiscard]] int32_t zerosLeft() const noexcept
    {
        return std::countl_zero(m_bits);
    }

    [[nodiscard]] int32_t onesRight() const noexcept
    {
        return std::countr_one(m_bits);
    }

    [[nodiscard]] int32_t onesLeft() const noexcept
    {
        return std::countl_one(m_bits);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_bits == 0;
    }

    [[nodiscard]] uint64_t data() const noexcept
    {
        return  m_bits;
    }

private:
    uint64_t m_bits;
};


}

#endif //SIMD_FIX_BITSET_HPP
