//
// Created by Fredrik Dahlberg on 2026-04-24.
//

#ifndef SIMD_FIX_BITSET_HPP
#define SIMD_FIX_BITSET_HPP
#include <cstdint>

namespace org::limitless::fix::parser {

class BitSet64
{
public:
    BitSet64() : m_bits(0ull) //
    {
    }

    BitSet64 operator>>=(size_t position) noexcept
    {
        m_bits >>= position;
        return *this;
    }

    BitSet64 operator<<=(size_t position) noexcept
    {
        m_bits <<= position;
        return *this;
    }

    BitSet64 set() noexcept
    {
        m_bits = static_cast<uint64_t>(-1);
        return *this;
    }

    BitSet64 set(uint32_t position)
    {
        m_bits |= 1 << position;
        return *this;
    }

    [[nodiscard]] uint64_t get(uint32_t position) const noexcept
    {
        return m_bits & (1 << position);
    }

    BitSet64 clear() noexcept
    {
        m_bits = 0;
        return *this;
    }

    BitSet64 clear(uint32_t position) noexcept
    {
        m_bits &= ~(1 << position);
        return *this;
    }

    [[nodiscard]] uint32_t zerosRight() const noexcept
    {
        return std::countr_zero(m_bits);
    }

    [[nodiscard]] uint32_t zerosLeft() const noexcept
    {
        return std::countl_zero(m_bits);
    }

    [[nodiscard]] uint32_t onesRight() const noexcept
    {
        return std::countr_one(m_bits);
    }

    [[nodiscard]] uint32_t onesLeft() const noexcept
    {
        return std::countl_one(m_bits);
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_bits == 0;
    }

private:
    uint64_t m_bits;
};


}

#endif //SIMD_FIX_BITSET_HPP
