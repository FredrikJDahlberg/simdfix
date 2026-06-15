//
// Created by Fredrik Dahlberg on 2026-06-15.
//
#include <gtest/gtest.h>

#include "../../../../../../main/cpp/org/limitless/fix/utils/FixedDecimal.hpp"

namespace org::limitless::fix::utils {

TEST(FixedDecimal, Construction)
{
    constexpr FixedDecimal value(12345, -2);
    ASSERT_EQ(12345, value.mantissa());
    ASSERT_EQ(-2, value.exponent());
}

TEST(FixedDecimal, Equality)
{
    ASSERT_EQ(FixedDecimal(12345, -2), FixedDecimal(123450, -3));
    ASSERT_EQ(FixedDecimal(0, 3), FixedDecimal(0, -5));
    ASSERT_NE(FixedDecimal(1, 0), FixedDecimal(10, -2));
}

TEST(FixedDecimal, Ordering)
{
    ASSERT_LT(FixedDecimal(99, -2), FixedDecimal(1, 0));
    ASSERT_GT(FixedDecimal(-1, 0), FixedDecimal(-200, -2));
    ASSERT_LE(FixedDecimal(5, -1), FixedDecimal(50, -2));
}

TEST(FixedDecimal, Addition)
{
    const FixedDecimal a(12345, -2); // 123.45
    const FixedDecimal b(155, -2);   // 1.55
    const auto sum = a + b;
    ASSERT_EQ(FixedDecimal(125, 0), sum); // 125
}

TEST(FixedDecimal, Subtraction)
{
    const FixedDecimal a(12345, -2); // 123.45
    const FixedDecimal b(155, -2);   // 1.55
    const auto diff = a - b;
    ASSERT_EQ(FixedDecimal(12190, -2), diff); // 121.90
}

TEST(FixedDecimal, UnaryMinus)
{
    const FixedDecimal a(12345, -2);
    ASSERT_EQ(FixedDecimal(-12345, -2), -a);
    ASSERT_EQ(a, -(-a));
}

TEST(FixedDecimal, Multiplication)
{
    const FixedDecimal price(1050, -2);  // 10.50
    const FixedDecimal qty(3, 0);        // 3
    ASSERT_EQ(FixedDecimal(3150, -2), price * qty); // 31.50
}

TEST(FixedDecimal, Division)
{
    const FixedDecimal a(1, 0); // 1
    const FixedDecimal b(4, 0); // 4
    ASSERT_EQ(FixedDecimal(25, -2), a / b); // 0.25

    const FixedDecimal c(10, 0);
    const FixedDecimal d(3, 0);
    const auto result = c / d; // 3.333333... rounded to the smallest representable exponent
    ASSERT_EQ(FixedDecimal::ExponentMin, result.exponent());
    ASSERT_EQ(333333333, result.mantissa()); // 3.33333333
}

TEST(FixedDecimal, DivisionByZero)
{
    const FixedDecimal a(10, 0);
    const FixedDecimal zero(0, 0);
    ASSERT_EQ(FixedDecimal(0, 0), a / zero);
}

TEST(FixedDecimal, CompoundAssignment)
{
    FixedDecimal value(100, -2); // 1.00
    value += FixedDecimal(50, -2);
    ASSERT_EQ(FixedDecimal(150, -2), value);

    value -= FixedDecimal(25, -2);
    ASSERT_EQ(FixedDecimal(125, -2), value);

    value *= FixedDecimal(2, 0);
    ASSERT_EQ(FixedDecimal(250, -2), value);

    value /= FixedDecimal(5, -1);
    ASSERT_EQ(FixedDecimal(500, -2), value);
}

TEST(FixedDecimal, RangeLimits)
{
    ASSERT_EQ(FixedDecimal::MantissaMax, FixedDecimal(FixedDecimal::MantissaMax, 0).mantissa());
    ASSERT_EQ(FixedDecimal::MantissaMin, FixedDecimal(FixedDecimal::MantissaMin, 0).mantissa());
    ASSERT_EQ(FixedDecimal::ExponentMax, FixedDecimal(1, FixedDecimal::ExponentMax).exponent());
    ASSERT_EQ(FixedDecimal::ExponentMin, FixedDecimal(1, FixedDecimal::ExponentMin).exponent());
}

TEST(FixedDecimal, OverflowSaturates)
{
    const auto result = FixedDecimal(FixedDecimal::MantissaMax, FixedDecimal::ExponentMax) +
                         FixedDecimal(FixedDecimal::MantissaMax, FixedDecimal::ExponentMax);
    ASSERT_EQ(FixedDecimal::ExponentMax, result.exponent());
    ASSERT_EQ(FixedDecimal::MantissaMax, result.mantissa());
}

TEST(FixedDecimal, OutOfRangeExponentIsNormalized)
{
    // exponent 10 is out of range; mantissa is scaled up to compensate.
    const FixedDecimal value(1, 10);
    ASSERT_LE(value.exponent(), FixedDecimal::ExponentMax);
    ASSERT_EQ(FixedDecimal(10'000'000'000LL, 0), value);
}

}