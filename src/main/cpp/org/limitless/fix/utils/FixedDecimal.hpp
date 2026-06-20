//
// Created by Fredrik Dahlberg on 2026-06-15.
//

#ifndef SIMD_FIX_FIXEDDECIMAL_HPP
#define SIMD_FIX_FIXEDDECIMAL_HPP

#include <compare>
#include <cstdint>
#include <ostream>
#include <iomanip>

namespace org::limitless::fix::utils {

class FixedDecimal
{
private:
    int64_t m_mantissa;

    static constexpr int64_t Scale = 100000000;
    static constexpr int64_t MaxSafeAdd = INT64_MAX;
    static constexpr int64_t MinSafeAdd = INT64_MIN;
    static constexpr int64_t MaxSafeDiv = INT64_MAX / Scale;

    static constexpr int64_t Max64BitProductLimit = 3037000499LL;
    static constexpr int64_t SignedReciprocal64 = 184467440738LL;
    static constexpr __int128_t SignedReciprocal128 = (static_cast<__int128_t>(0x2AF31D) << 48) | 0xC4611873BF40ULL;
    static constexpr uint64_t Reciprocal = 184467440737ULL;

    /**
     * Extracts the integer part of the fixed-point mantissa using
     * reciprocal multiplication instead of a hardware division instruction.
     */
    static constexpr int64_t fastDivide100000000(const int64_t mantissa)
    {
        unsigned __int128 product = static_cast<unsigned __int128>(mantissa) * Reciprocal;
        return static_cast<int64_t>(product >> 64);
    }

public:
    static constexpr int64_t MantissaMin = INT64_MIN;
    static constexpr int64_t MantissaMax = INT64_MAX;

    constexpr FixedDecimal() : m_mantissa(0)
    {
    }

    explicit constexpr FixedDecimal(const int64_t mantissa) : m_mantissa(mantissa)
    {
    }

    constexpr FixedDecimal(const int64_t mantissa, const int32_t exponent)
    {
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

    [[nodiscard]] constexpr int64_t mantissa() const
    {
        return m_mantissa;
    }

    [[nodiscard]] constexpr double toDouble() const
    {
        return static_cast<double>(m_mantissa) / static_cast<double>(Scale);
    }

    constexpr FixedDecimal operator-() const
    {
        return FixedDecimal(-m_mantissa);
    }

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
        if (std::abs(m_mantissa) <= Max64BitProductLimit && std::abs(other.m_mantissa) <= Max64BitProductLimit) [[likely]]
        {
            const int64_t intermediate64 = m_mantissa * other.m_mantissa;
            __int128 product = static_cast<__int128>(intermediate64) * SignedReciprocal64;
            const int64_t upper_bits = static_cast<int64_t>(product >> 64);
            const int64_t correction = static_cast<int64_t>(static_cast<uint64_t>(intermediate64) >> 63);
            return FixedDecimal(upper_bits + correction);
        }

        const __int128_t intermediate128 = static_cast<__int128_t>(m_mantissa) * other.m_mantissa;
        const __int128_t divided = (intermediate128 * SignedReciprocal128) >> 96;
        const int64_t correction = static_cast<int64_t>(static_cast<uint64_t>(intermediate128 >> 127));
        return FixedDecimal(static_cast<int64_t>(divided) + correction);
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

        const __int128_t scaledNumber = static_cast<__int128_t>(m_mantissa) * Scale;
        return FixedDecimal(static_cast<int64_t>(scaledNumber / other.m_mantissa));
    }

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

    constexpr auto operator<=>(const FixedDecimal& other) const = default;

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