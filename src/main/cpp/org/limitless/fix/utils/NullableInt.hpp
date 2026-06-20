//
// Created by Fredrik Dahlberg on 2026-06-19.
//

#ifndef SIMD_FIX_NULLABLE_INT_HPP
#define SIMD_FIX_NULLABLE_INT_HPP

#include <compare>
#include <concepts>
#include <limits>
#include <ostream>

namespace org::limitless::fix::utils {

/**
 * A nullable integer type using a sentinel value for the null state.
 * Signed types use min() as the sentinel; unsigned types use max().
 * Null propagates through arithmetic and bitwise operations. Comparisons
 * follow std::optional ordering: null sorts before any value, and two
 * nulls compare equal.
 * @tparam T the underlying integer type
 */
template <std::integral T>
class NullableInt
{
public:
    using ValueType = T;

    static constexpr T NullValue = std::is_signed_v<T>
        ? std::numeric_limits<T>::min()
        : std::numeric_limits<T>::max();

    constexpr NullableInt() = default;

    constexpr NullableInt(const T value) : m_value{value}
    {
    }

    /**
     * @return a null instance
     */
    [[nodiscard]] static constexpr NullableInt null()
    {
        return NullableInt{};
    }

    /**
     * @return true if this instance holds a value
     */
    [[nodiscard]] constexpr bool hasValue() const
    {
        return m_value != NullValue;
    }

    /**
     * @return the underlying value (undefined if null)
     */
    [[nodiscard]] constexpr T value() const
    {
        return m_value;
    }

    /**
     * @param defaultValue value to return when null
     * @return the held value, or defaultValue if null
     */
    [[nodiscard]] constexpr T valueOr(const T defaultValue) const
    {
        return hasValue() ? m_value : defaultValue;
    }

    /**
     * Resets this instance to null.
     */
    constexpr void reset()
    {
        m_value = NullValue;
    }

    // --- UNARY OPERATORS ---

    constexpr NullableInt operator-() const
    {
        return hasValue() ? NullableInt{static_cast<T>(-m_value)} : null();
    }

    constexpr NullableInt operator+() const
    {
        return *this;
    }

    constexpr NullableInt operator~() const
    {
        return hasValue() ? NullableInt{static_cast<T>(~m_value)} : null();
    }

    // --- ARITHMETIC OPERATORS ---

    constexpr NullableInt operator+(const NullableInt& other) const
    {
        if (!hasValue() || !other.hasValue())
        {
            return null();
        }
        return NullableInt{static_cast<T>(m_value + other.m_value)};
    }

    constexpr NullableInt operator-(const NullableInt& other) const
    {
        if (!hasValue() || !other.hasValue())
        {
            return null();
        }
        return NullableInt{static_cast<T>(m_value - other.m_value)};
    }

    constexpr NullableInt operator*(const NullableInt& other) const
    {
        if (!hasValue() || !other.hasValue())
        {
            return null();
        }
        return NullableInt{static_cast<T>(m_value * other.m_value)};
    }

    constexpr NullableInt operator/(const NullableInt& other) const
    {
        if (!hasValue() || !other.hasValue() || other.m_value == 0)
        {
            return null();
        }
        return NullableInt{static_cast<T>(m_value / other.m_value)};
    }

    constexpr NullableInt operator%(const NullableInt& other) const
    {
        if (!hasValue() || !other.hasValue() || other.m_value == 0)
        {
            return null();
        }
        return NullableInt{static_cast<T>(m_value % other.m_value)};
    }

    // --- COMPOUND ASSIGNMENT OPERATORS ---

    constexpr NullableInt& operator+=(const NullableInt& other)
    {
        *this = *this + other;
        return *this;
    }

    constexpr NullableInt& operator-=(const NullableInt& other)
    {
        *this = *this - other;
        return *this;
    }

    constexpr NullableInt& operator*=(const NullableInt& other)
    {
        *this = *this * other;
        return *this;
    }

    constexpr NullableInt& operator/=(const NullableInt& other)
    {
        *this = *this / other;
        return *this;
    }

    constexpr NullableInt& operator%=(const NullableInt& other)
    {
        *this = *this % other;
        return *this;
    }

    // --- BITWISE OPERATORS ---

    constexpr NullableInt operator&(const NullableInt& other) const
    {
        if (!hasValue() || !other.hasValue())
        {
            return null();
        }
        return NullableInt{static_cast<T>(m_value & other.m_value)};
    }

    constexpr NullableInt operator|(const NullableInt& other) const
    {
        if (!hasValue() || !other.hasValue())
        {
            return null();
        }
        return NullableInt{static_cast<T>(m_value | other.m_value)};
    }

    constexpr NullableInt operator^(const NullableInt& other) const
    {
        if (!hasValue() || !other.hasValue())
        {
            return null();
        }
        return NullableInt{static_cast<T>(m_value ^ other.m_value)};
    }

    constexpr NullableInt operator<<(const NullableInt& other) const
    {
        if (!hasValue() || !other.hasValue())
        {
            return null();
        }
        return NullableInt{static_cast<T>(m_value << other.m_value)};
    }

    constexpr NullableInt operator>>(const NullableInt& other) const
    {
        if (!hasValue() || !other.hasValue())
        {
            return null();
        }
        return NullableInt{static_cast<T>(m_value >> other.m_value)};
    }

    // --- BITWISE COMPOUND ASSIGNMENT ---

    constexpr NullableInt& operator&=(const NullableInt& other)
    {
        *this = *this & other;
        return *this;
    }

    constexpr NullableInt& operator|=(const NullableInt& other)
    {
        *this = *this | other;
        return *this;
    }

    constexpr NullableInt& operator^=(const NullableInt& other)
    {
        *this = *this ^ other;
        return *this;
    }

    constexpr NullableInt& operator<<=(const NullableInt& other)
    {
        *this = *this << other;
        return *this;
    }

    constexpr NullableInt& operator>>=(const NullableInt& other)
    {
        *this = *this >> other;
        return *this;
    }

    // --- RELATIONAL OPERATORS ---

    constexpr bool operator==(const NullableInt& other) const = default;

    constexpr std::strong_ordering operator<=>(const NullableInt& other) const
    {
        if (!hasValue() && !other.hasValue())
        {
            return std::strong_ordering::equal;
        }
        if (!hasValue())
        {
            return std::strong_ordering::less;
        }
        if (!other.hasValue())
        {
            return std::strong_ordering::greater;
        }
        return m_value <=> other.m_value;
    }

    // --- STREAM OPERATOR ---

    friend std::ostream& operator<<(std::ostream& os, const NullableInt& value)
    {
        if (value.hasValue())
        {
            os << +value.m_value;
        }
        else
        {
            os << "null";
        }
        return os;
    }

private:
    T m_value{NullValue};
};

}

#endif //SIMD_FIX_NULLABLE_INT_HPP