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
#if 0
class FixedDecimal
{
private:
    int64_t m_mantissa : 60;
    int64_t m_exponent : 4;

    static constexpr int64_t Scale = 100000000;
    static constexpr int8_t  TargetExp = -8;

    static constexpr int64_t MaxSafeAdd = (1LL << 59) - 1;
    static constexpr int64_t MaxSafeDiv = (1ULL << 38) - 1;

    constexpr static int64_t alignToTarget(const int64_t mantissa, const int8_t currentExp)
    {
        if (currentExp < -8 || currentExp > 7)
        {
            throw std::out_of_range("Exponent must be between -8 and 7.");
        }

        const int8_t diff = currentExp - TargetExp;
        if (diff == 0)
        {
            return mantissa;
        }

        int64_t multiplier = 1;
        if (diff > 0)
        {
            for (int i = 0; i < diff; ++i)
            {
                multiplier *= 10;
            }
            return mantissa * multiplier;
        }
        else
        {
            for (int i = 0; i < -diff; ++i)
            {
                multiplier *= 10;
            }
            return mantissa / multiplier;
        }
    }

public:
    // --- PUBLIC FORMAT BOUNDS CONSTANTS (C++17 Constexpr) ---

    // Lower limit of a signed 60-bit integer: -2^59
    static constexpr int64_t MantissaMin = -(1LL << 59);

    // Upper limit of a signed 60-bit integer: (2^59) - 1
    static constexpr int64_t MantissaMax = (1LL << 59) - 1;

    // Naturally maps to -8 for a 4-bit signed integer
    static constexpr int8_t  ExponentMin = -8;

    // Naturally maps to 7 for a 4-bit signed integer
    static constexpr int8_t  ExponentMax = 7;


    constexpr FixedDecimal() : m_mantissa(0), m_exponent(TargetExp)
    {
    }

    constexpr FixedDecimal(const int64_t mantissa, const int8_t exponent)
    {
        m_exponent = TargetExp;
        m_mantissa = alignToTarget(mantissa, exponent);
    }

    constexpr FixedDecimal(const int64_t mantissa, const int32_t exponent)
        : FixedDecimal(mantissa, static_cast<int8_t>(exponent))
    {
    }

    static FixedDecimal of(const int64_t mantissa, const int8_t exponent)
    {
        if (exponent < -8 || exponent > 7)
        {
            throw std::out_of_range("Raw exponent must be between -8 and 7.");
        }
        FixedDecimal decimalInstance;
        decimalInstance.m_mantissa = mantissa;
        decimalInstance.m_exponent = exponent;
        return decimalInstance;
    }

    int64_t mantissa() const
    {
        return m_mantissa;
    }

    int8_t  exponent() const
    {
        return m_exponent;
    }

    double toDouble() const
    {
        constexpr static double powersOf10[] =
        {
            1e-8, 1e-7, 1e-6, 1e-5, 1e-4, 1e-3, 1e-2, 1e-1,
            1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7
        };
        return static_cast<double>(m_mantissa) * powersOf10[m_exponent + 8];
    }

    // --- ARITHMETIC OPERATORS ---
    // Unary minus operator (-) to negate a FixedDecimal instance
    constexpr FixedDecimal operator-() const
    {
        // Simply negating the internal 60-bit mantissa preserves the -8 normalized scaling
        return of(-m_mantissa, TargetExp);
    }

    FixedDecimal operator+(const FixedDecimal& other) const
    {
        const int64_t absA = (m_mantissa < 0) ? -m_mantissa : m_mantissa;
        const int64_t absB = (other.m_mantissa < 0) ? -other.m_mantissa : other.m_mantissa;
        if (absA < MaxSafeAdd && absB < MaxSafeAdd)
        {
            return of(m_mantissa + other.m_mantissa, TargetExp);
        }
        __int128_t result = static_cast<__int128_t>(m_mantissa) + other.m_mantissa;
        return of(static_cast<int64_t>(result), TargetExp);
    }

    FixedDecimal operator-(const FixedDecimal& other) const
    {
        const int64_t absA = (m_mantissa < 0) ? -m_mantissa : m_mantissa;
        const int64_t absB = (other.m_mantissa < 0) ? -other.m_mantissa : other.m_mantissa;
        if (absA < MaxSafeAdd && absB < MaxSafeAdd)
        {
            return of(m_mantissa - other.m_mantissa, TargetExp);
        }
        __int128_t result = static_cast<__int128_t>(m_mantissa) - other.m_mantissa;
        return of(static_cast<int64_t>(result), TargetExp);
    }

    FixedDecimal operator*(const FixedDecimal& other) const
    {
        const __int128_t intermediate = static_cast<__int128_t>(m_mantissa) * other.m_mantissa;
        const int64_t finalMantissa = static_cast<int64_t>(intermediate / Scale);
        return of(finalMantissa, TargetExp);
    }

    FixedDecimal operator/(const FixedDecimal& other) const
    {
        if (other.m_mantissa == 0)
        {
            throw std::runtime_error("Division by zero.");
        }

        const int64_t absNum = (m_mantissa < 0) ? -m_mantissa : m_mantissa;
        if (absNum <= MaxSafeDiv)
        {
            return of((m_mantissa * Scale) / other.m_mantissa, TargetExp);
        }

        __int128_t scaledNum = static_cast<__int128_t>(m_mantissa) * Scale;
        int64_t finalMantissa = static_cast<int64_t>(scaledNum / other.m_mantissa);
        return of(finalMantissa, TargetExp);
    }

    // --- COMPOUND ASSIGNMENT OPERATORS ---

    constexpr FixedDecimal& operator+=(const FixedDecimal& other)
    {
        *this = *this + other;
        return *this;
    }

    constexpr FixedDecimal& operator-=(const FixedDecimal& other)
    {
        *this = *this - other;
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

    bool operator>(const FixedDecimal& other) const
    {
        return m_mantissa > other.m_mantissa;
    }

    bool operator>=(const FixedDecimal& other) const
    {
        return m_mantissa >= other.m_mantissa;
    }

    // --- STREAM OPERATOR ---

    friend std::ostream& operator<<(std::ostream& os, const FixedDecimal& fd)
    {
        int64_t localMantissa = fd.m_mantissa;
        if (localMantissa < 0)
        {
            os << '-';
            localMantissa = -localMantissa;
        }

        const int64_t wholePart = localMantissa / Scale;
        const int64_t fractionPart = localMantissa % Scale;
        os << wholePart << '.' << std::setfill('0') << std::setw(8) << fractionPart;
        return os;
    }
};
#endif
} // namespace Trading::Math

#endif //SIMD_FIX_FIXEDDECIMAL_HPP