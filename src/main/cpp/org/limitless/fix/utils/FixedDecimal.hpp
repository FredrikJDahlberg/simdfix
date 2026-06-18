//
// Created by Fredrik Dahlberg on 2026-06-15.
//

#ifndef SIMD_FIX_FIXEDDECIMAL_HPP
#define SIMD_FIX_FIXEDDECIMAL_HPP

#include <cstdint>
#include <ostream>
#include <iomanip>
#include <stdexcept>

namespace org::limitless::fix::utils {

class FixedDecimal
{
private:
    // Pure 64-bit storage. No bitfield slicing or shifting required.
    int64_t m_mantissa;

    // Hard-locked at 8 decimal places (10^8) for absolute fixed-income safety
    static constexpr int64_t Scale = 100000000;

    // Overflow safety thresholds for native 64-bit integer limits
    static constexpr int64_t MaxSafeAdd = INT64_MAX;
    static constexpr int64_t MinSafeAdd = INT64_MIN;
    static constexpr int64_t MaxSafeDiv = INT64_MAX / Scale;

    // Reciprocal equivalent to: floor(2^64 / 10^8)
    static constexpr uint64_t Reciprocal = 184467440737ULL;

    /**
     * Extracts the integer part of the fixed-point mantissa using
     * reciprocal multiplication instead of a hardware division instruction.
     */
    static constexpr int64_t fastDivide100000000(const int64_t mantissa)
    {
        // Force unsigned widening for the scaling phase
        unsigned __int128 product = static_cast<unsigned __int128>(mantissa) * Reciprocal;

        // Shift right by 64 bits to complete the reciprocal division
        // On M1 Pro, this maps straight to extracting the upper register
        return static_cast<int64_t>(product >> 64);
    }

public:
    // --- PUBLIC FORMAT BOUNDS CONSTANTS ---
    static constexpr int64_t MantissaMin = INT64_MIN;
    static constexpr int64_t MantissaMax = INT64_MAX;

    // Default constructor
    constexpr FixedDecimal() : m_mantissa(0)
    {
    }

    // Explicit constructor from raw 10^-8 integer internal values
    explicit constexpr FixedDecimal(const int64_t mantissa) : m_mantissa(mantissa)
    {
    }

    // Constructor that parses mantissa/exponent structures from your input loop
    constexpr FixedDecimal(const int64_t mantissa, const int32_t exponent)
    {
        // Target exponent is hard-locked to -8
        const int32_t diff = exponent - (-8);
        if (diff == 0)
        {
            m_mantissa = mantissa;
        }
        else if (diff > 0)
        {
            int64_t multiplier = 1;
            for (int i = 0; i < diff; ++i)
            {
                multiplier *= 10;
            }
            m_mantissa = mantissa * multiplier;
        }
        else
        {
            int64_t divisor = 1;
            for (int i = 0; i < -diff; ++i)
            {
                divisor *= 10;
            }
            m_mantissa = mantissa / divisor;
        }
    }

    // Serialization utility accessors
    int64_t mantissa() const
    {
        return m_mantissa;
    }

    double toDouble() const
    {
        return static_cast<double>(m_mantissa) / static_cast<double>(Scale);
    }

    // --- UNARY OPERATORS ---

    constexpr FixedDecimal operator-() const
    {
        return FixedDecimal(-m_mantissa);
    }

    // --- ARITHMETIC OPERATORS (Native 1-Cycle ARM64 Execution) ---

    constexpr FixedDecimal operator+(const FixedDecimal& other) const
    {
        return FixedDecimal(m_mantissa + other.m_mantissa);
    }

    constexpr FixedDecimal operator-(const FixedDecimal& other) const
    {
        return FixedDecimal(m_mantissa - other.m_mantissa);
    }

    constexpr FixedDecimal operator*(const FixedDecimal& other) const
    {
        // Intermediate promotes to 128-bit only for the multiplication step
        __int128_t intermediate = static_cast<__int128_t>(m_mantissa) * other.m_mantissa;
        // FIXME: fast div
        return FixedDecimal(static_cast<int64_t>(intermediate / Scale));
    }

    constexpr FixedDecimal operator/(const FixedDecimal& other) const
    {
        if (other.m_mantissa == 0)
        {
            // throw std::runtime_error("Division by zero.");
            return other;
        }

        const int64_t absNum = (m_mantissa < 0) ? -m_mantissa : m_mantissa;
        if (absNum <= MaxSafeDiv)
        {
            return FixedDecimal((m_mantissa * Scale) / other.m_mantissa);
        }

        // Software precision protection for maximum range limits
        const __int128_t scaledNumber = static_cast<__int128_t>(m_mantissa) * Scale;
        return FixedDecimal(static_cast<int64_t>(scaledNumber / other.m_mantissa));
    }

    // --- COMPOUND ASSIGNMENT OPERATORS ---

    constexpr FixedDecimal& operator+=(const FixedDecimal& other)
    {
        m_mantissa += other.m_mantissa;
        return *this;
    }

    constexpr FixedDecimal& operator-=(const FixedDecimal& other)
    {
        m_mantissa -= other.m_mantissa;
        return *this;
    }

    constexpr FixedDecimal& operator*=(const FixedDecimal& other)
    {
        *this = *this * other;
        return *this;
    }

    constexpr FixedDecimal& operator/=(const FixedDecimal& other)
    {
        *this = *this / other;
        return *this;
    }

    // --- RELATIONAL OPERATORS ---

    bool operator==(const FixedDecimal& other) const
    {
        return m_mantissa == other.m_mantissa;
    }

    bool operator!=(const FixedDecimal& other) const
    {
        return m_mantissa != other.m_mantissa;
    }

    bool operator<(const FixedDecimal& other) const
    {
        return m_mantissa < other.m_mantissa;
    }

    bool operator<=(const FixedDecimal& other) const
    {
        return m_mantissa <= other.m_mantissa;
    }

    bool operator>=(const FixedDecimal& other) const
    {
        return m_mantissa >= other.m_mantissa;
    }

    bool operator>(const FixedDecimal& other) const
    {
        return m_mantissa > other.m_mantissa;
    }

    // --- STREAM OPERATOR ---

    friend std::ostream& operator<<(std::ostream& os, const FixedDecimal& value)
    {
        int64_t mantissa = value.m_mantissa;
        if (mantissa < 0)
        {
            os << '-';
            mantissa = -mantissa;
        }

        const int64_t integer = mantissa / Scale;
        const int64_t fraction = mantissa % Scale;
        os << integer << '.' << std::setfill('0') << std::setw(8) << fraction;
        return os;
    }
};

}

#endif //SIMD_FIX_FIXEDDECIMAL_HPP