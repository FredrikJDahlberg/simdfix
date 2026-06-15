//
// Created by Fredrik Dahlberg on 2026-06-15.
//

#ifndef SIMD_FIX_FIXEDDECIMAL_HPP
#define SIMD_FIX_FIXEDDECIMAL_HPP

#include <cstdint>
#include <compare>
#include <utility>

namespace org::limitless::fix::utils {

/**
 * Fixed-point decimal value backed by a single uint64_t: a signed 60-bit
 * mantissa packed into the low bits and a signed 4-bit exponent packed into
 * the high bits. The represented value is mantissa * 10^exponent.
 *
 * Used for FIX `float` / `Qty` / `Price` / `PriceOffset` / `Amt` /
 * `Percentage` fields without floating point.
 */
class FixedDecimal
{
public:
    static constexpr int32_t MantissaBits = 60;
    static constexpr int32_t ExponentBits = 4;
    static constexpr int64_t MantissaMax = (int64_t{1} << (MantissaBits - 1)) - 1;
    static constexpr int64_t MantissaMin = -(int64_t{1} << (MantissaBits - 1));
    static constexpr int32_t ExponentMax = (int32_t{1} << (ExponentBits - 1)) - 1;
    static constexpr int32_t ExponentMin = -(int32_t{1} << (ExponentBits - 1));

    constexpr FixedDecimal() noexcept : m_bits(0)
    {
    }

    /**
     * @param mantissa signed mantissa
     * @param exponent power-of-ten exponent applied to mantissa
     *
     * mantissa * 10^exponent is rounded and clamped to fit the 60-bit
     * mantissa / 4-bit exponent ranges if it does not fit exactly.
     */
    constexpr FixedDecimal(const int64_t mantissa, const int32_t exponent) noexcept
        : m_bits(normalize(mantissa, exponent).m_bits)
    {
    }

    /**
     * @return the signed mantissa, in [MantissaMin, MantissaMax]
     */
    [[nodiscard]] constexpr int64_t mantissa() const noexcept
    {
        return static_cast<int64_t>(m_bits << ExponentBits) >> ExponentBits;
    }

    /**
     * @return the signed power-of-ten exponent, in [ExponentMin, ExponentMax]
     */
    [[nodiscard]] constexpr int32_t exponent() const noexcept
    {
        return static_cast<int32_t>(static_cast<int64_t>(m_bits) >> MantissaBits);
    }

    /**
     * @return the underlying packed bit pattern
     */
    [[nodiscard]] constexpr uint64_t data() const noexcept
    {
        return m_bits;
    }

    [[nodiscard]] constexpr FixedDecimal operator-() const noexcept
    {
        return normalize(-static_cast<__int128>(mantissa()), exponent());
    }

    constexpr FixedDecimal& operator+=(const FixedDecimal& other) noexcept
    {
        return *this = *this + other;
    }

    constexpr FixedDecimal& operator-=(const FixedDecimal& other) noexcept
    {
        return *this = *this - other;
    }

    constexpr FixedDecimal& operator*=(const FixedDecimal& other) noexcept
    {
        return *this = *this * other;
    }

    constexpr FixedDecimal& operator/=(const FixedDecimal& other) noexcept
    {
        return *this = *this / other;
    }

    [[nodiscard]] friend constexpr FixedDecimal operator+(const FixedDecimal& lhs, const FixedDecimal& rhs) noexcept
    {
        int32_t exponent;
        const auto [a, b] = align(lhs, rhs, exponent);
        return normalize(a + b, exponent);
    }

    [[nodiscard]] friend constexpr FixedDecimal operator-(const FixedDecimal& lhs, const FixedDecimal& rhs) noexcept
    {
        return lhs + -rhs;
    }

    [[nodiscard]] friend constexpr FixedDecimal operator*(const FixedDecimal& lhs, const FixedDecimal& rhs) noexcept
    {
        const __int128 product = static_cast<__int128>(lhs.mantissa()) * rhs.mantissa();
        return normalize(product, lhs.exponent() + rhs.exponent());
    }

    /**
     * @return lhs / rhs, rounded to the nearest representable value; 0 if rhs is 0
     */
    [[nodiscard]] friend constexpr FixedDecimal operator/(const FixedDecimal& lhs, const FixedDecimal& rhs) noexcept
    {
        constexpr __int128 Scale = static_cast<__int128>(1'000'000'000LL) * 1'000'000'000LL; // 10^18
        constexpr int32_t ScalePow = 18;

        const __int128 denominator = rhs.mantissa();
        if (denominator == 0)
        {
            return FixedDecimal{};
        }
        const __int128 numerator = static_cast<__int128>(lhs.mantissa()) * Scale;
        __int128 quotient = numerator / denominator;
        const __int128 remainder = numerator % denominator;
        if (remainder != 0)
        {
            __int128 absRemainderX2 = remainder < 0 ? -remainder : remainder;
            absRemainderX2 += absRemainderX2;
            const __int128 absDenominator = denominator < 0 ? -denominator : denominator;
            if (absRemainderX2 >= absDenominator)
            {
                quotient += (numerator < 0) == (denominator < 0) ? 1 : -1;
            }
        }
        return normalize(quotient, lhs.exponent() - rhs.exponent() - ScalePow);
    }

    [[nodiscard]] friend constexpr bool operator==(const FixedDecimal& lhs, const FixedDecimal& rhs) noexcept
    {
        int32_t exponent;
        const auto [a, b] = align(lhs, rhs, exponent);
        return a == b;
    }

    [[nodiscard]] friend constexpr std::strong_ordering operator<=>(const FixedDecimal& lhs, const FixedDecimal& rhs) noexcept
    {
        int32_t exponent;
        const auto [a, b] = align(lhs, rhs, exponent);
        return a <=> b;
    }

private:
    static constexpr uint64_t MantissaMask = (uint64_t{1} << MantissaBits) - 1;
    static constexpr uint64_t ExponentMask = (uint64_t{1} << ExponentBits) - 1;

    explicit constexpr FixedDecimal(const uint64_t bits) noexcept : m_bits(bits)
    {
    }

    /**
     * Rounds mantissa * 10^exponent to the nearest value representable as a
     * 60-bit mantissa with a 4-bit exponent, saturating at the representable
     * extremes if it is out of range.
     */
    [[nodiscard]] static constexpr FixedDecimal normalize(__int128 mantissa, int32_t exponent) noexcept
    {
        while (exponent > ExponentMax)
        {
            if (mantissa > MantissaMax / 10 || mantissa < MantissaMin / 10)
            {
                mantissa = mantissa > 0 ? MantissaMax : mantissa < 0 ? MantissaMin : 0;
                exponent = ExponentMax;
                break;
            }
            mantissa *= 10;
            --exponent;
        }
        while (mantissa > MantissaMax || mantissa < MantissaMin || exponent < ExponentMin)
        {
            if (exponent >= ExponentMax)
            {
                mantissa = mantissa > 0 ? MantissaMax : MantissaMin;
                exponent = ExponentMax;
                break;
            }
            const __int128 remainder = mantissa % 10;
            mantissa /= 10;
            if (remainder >= 5)
            {
                ++mantissa;
            }
            else if (remainder <= -5)
            {
                --mantissa;
            }
            ++exponent;
        }
        const uint64_t bits = (static_cast<uint64_t>(static_cast<int64_t>(mantissa)) & MantissaMask) |
                              (static_cast<uint64_t>(exponent) & ExponentMask) << MantissaBits;
        return FixedDecimal(bits);
    }

    /**
     * @return the mantissas of lhs and rhs scaled to a common exponent (the
     *         smaller of the two), which is written to exponent
     */
    [[nodiscard]] static constexpr std::pair<__int128, __int128> align(
        const FixedDecimal& lhs, const FixedDecimal& rhs, int32_t& exponent) noexcept
    {
        exponent = lhs.exponent() < rhs.exponent() ? lhs.exponent() : rhs.exponent();
        __int128 a = lhs.mantissa();
        __int128 b = rhs.mantissa();
        for (int32_t i = exponent; i < lhs.exponent(); ++i)
        {
            a *= 10;
        }
        for (int32_t i = exponent; i < rhs.exponent(); ++i)
        {
            b *= 10;
        }
        return {a, b};
    }

    uint64_t m_bits;
};

}

#endif //SIMD_FIX_FIXEDDECIMAL_HPP