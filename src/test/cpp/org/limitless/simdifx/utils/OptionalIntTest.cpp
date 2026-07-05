//
// Created by Fredrik Dahlberg on 2026-06-19.
//
#include <gtest/gtest.h>
#include <sstream>

#include "../../../../../../main/cpp/org/limitless/simdifx/utils/OptionalInt.hpp"

namespace org::limitless::simdifx::utils {

using NullInt32 = OptionalInt<int32_t>;
using NullUint32 = OptionalInt<uint32_t>;
using NullInt64 = OptionalInt<int64_t>;
using NullUint64 = OptionalInt<uint64_t>;

// --- Sentinel values ---

TEST(OptionalInt, SignedSentinelIsMin)
{
    static_assert(NullInt32::NullValue == std::numeric_limits<int32_t>::min());
    static_assert(NullInt64::NullValue == std::numeric_limits<int64_t>::min());
}

TEST(OptionalInt, UnsignedSentinelIsMax)
{
    static_assert(NullUint32::NullValue == std::numeric_limits<uint32_t>::max());
    static_assert(NullUint64::NullValue == std::numeric_limits<uint64_t>::max());
}

// --- Construction and accessors ---

TEST(OptionalInt, DefaultIsNull)
{
    constexpr NullInt32 value;
    ASSERT_FALSE(value.hasValue());
}

TEST(OptionalInt, ValueConstruction)
{
    constexpr NullInt32 value{42};
    ASSERT_TRUE(value.hasValue());
    ASSERT_EQ(42, value.value());
}

TEST(OptionalInt, ZeroIsNotNull)
{
    constexpr NullInt32 value{0};
    ASSERT_TRUE(value.hasValue());
    ASSERT_EQ(0, value.value());
}

TEST(OptionalInt, NullFactory)
{
    constexpr auto value = NullInt32::null();
    ASSERT_FALSE(value.hasValue());
}

TEST(OptionalInt, ValueOr)
{
    constexpr NullInt32 present{10};
    constexpr NullInt32 absent;
    ASSERT_EQ(10, present.valueOr(99));
    ASSERT_EQ(99, absent.valueOr(99));
}

TEST(OptionalInt, Reset)
{
    NullInt32 value{42};
    ASSERT_TRUE(value.hasValue());
    value.reset();
    ASSERT_FALSE(value.hasValue());
}

// --- Unary operators ---

TEST(OptionalInt, UnaryMinus)
{
    constexpr NullInt32 a{7};
    ASSERT_EQ(-7, (-a).value());
    ASSERT_EQ(a, -(-a));

    constexpr auto nullResult = -NullInt32::null();
    ASSERT_FALSE(nullResult.hasValue());
}

TEST(OptionalInt, UnaryPlus)
{
    constexpr NullInt32 a{7};
    ASSERT_EQ(a, +a);
}

TEST(OptionalInt, BitwiseNot)
{
    constexpr NullUint32 a{0x0F};
    ASSERT_TRUE((~a).hasValue());
    ASSERT_EQ(static_cast<uint32_t>(~0x0Fu), (~a).value());

    ASSERT_FALSE((~NullUint32::null()).hasValue());
}

// --- Arithmetic operators ---

TEST(OptionalInt, Addition)
{
    constexpr NullInt32 a{10};
    constexpr NullInt32 b{20};
    ASSERT_EQ(30, (a + b).value());
}

TEST(OptionalInt, AdditionNullPropagates)
{
    constexpr NullInt32 a{10};
    constexpr NullInt32 n;
    ASSERT_FALSE((a + n).hasValue());
    ASSERT_FALSE((n + a).hasValue());
    ASSERT_FALSE((n + n).hasValue());
}

TEST(OptionalInt, Subtraction)
{
    constexpr NullInt32 a{30};
    constexpr NullInt32 b{12};
    ASSERT_EQ(18, (a - b).value());
}

TEST(OptionalInt, Multiplication)
{
    constexpr NullInt32 a{6};
    constexpr NullInt32 b{7};
    ASSERT_EQ(42, (a * b).value());
}

TEST(OptionalInt, Division)
{
    constexpr NullInt32 a{100};
    constexpr NullInt32 b{4};
    ASSERT_EQ(25, (a / b).value());
}

TEST(OptionalInt, DivisionByZeroIsNull)
{
    constexpr NullInt32 a{10};
    constexpr NullInt32 zero{0};
    ASSERT_FALSE((a / zero).hasValue());
}

TEST(OptionalInt, Modulo)
{
    constexpr NullInt32 a{17};
    constexpr NullInt32 b{5};
    ASSERT_EQ(2, (a % b).value());
}

TEST(OptionalInt, ModuloByZeroIsNull)
{
    constexpr NullInt32 a{10};
    constexpr NullInt32 zero{0};
    ASSERT_FALSE((a % zero).hasValue());
}

// --- Compound assignment ---

TEST(OptionalInt, CompoundAssignment)
{
    NullInt32 value{100};

    value += NullInt32{50};
    ASSERT_EQ(150, value.value());

    value -= NullInt32{25};
    ASSERT_EQ(125, value.value());

    value *= NullInt32{2};
    ASSERT_EQ(250, value.value());

    value /= NullInt32{5};
    ASSERT_EQ(50, value.value());

    value %= NullInt32{7};
    ASSERT_EQ(1, value.value());
}

TEST(OptionalInt, CompoundAssignmentNullPropagates)
{
    NullInt32 value{100};
    value += NullInt32::null();
    ASSERT_FALSE(value.hasValue());
}

// --- Bitwise operators ---

TEST(OptionalInt, BitwiseAnd)
{
    constexpr NullUint32 a{0xFF};
    constexpr NullUint32 b{0x0F};
    ASSERT_EQ(0x0Fu, (a & b).value());
}

TEST(OptionalInt, BitwiseOr)
{
    constexpr NullUint32 a{0xF0};
    constexpr NullUint32 b{0x0F};
    ASSERT_EQ(0xFFu, (a | b).value());
}

TEST(OptionalInt, BitwiseXor)
{
    constexpr NullUint32 a{0xFF};
    constexpr NullUint32 b{0x0F};
    ASSERT_EQ(0xF0u, (a ^ b).value());
}

TEST(OptionalInt, ShiftLeft)
{
    constexpr NullUint32 a{1};
    constexpr NullUint32 b{4};
    ASSERT_EQ(16u, (a << b).value());
}

TEST(OptionalInt, ShiftRight)
{
    constexpr NullUint32 a{16};
    constexpr NullUint32 b{4};
    ASSERT_EQ(1u, (a >> b).value());
}

TEST(OptionalInt, BitwiseNullPropagates)
{
    constexpr NullUint32 a{0xFF};
    constexpr NullUint32 n;
    ASSERT_FALSE((a & n).hasValue());
    ASSERT_FALSE((a | n).hasValue());
    ASSERT_FALSE((a ^ n).hasValue());
    ASSERT_FALSE((a << n).hasValue());
    ASSERT_FALSE((a >> n).hasValue());
}

TEST(OptionalInt, BitwiseCompoundAssignment)
{
    NullUint32 value{0xFF};

    value &= NullUint32{0x0F};
    ASSERT_EQ(0x0Fu, value.value());

    value |= NullUint32{0xF0};
    ASSERT_EQ(0xFFu, value.value());

    value ^= NullUint32{0x0F};
    ASSERT_EQ(0xF0u, value.value());

    value <<= NullUint32{4};
    ASSERT_EQ(0xF00u, value.value());

    value >>= NullUint32{8};
    ASSERT_EQ(0x0Fu, value.value());
}

// --- Relational operators ---

TEST(OptionalInt, Equality)
{
    constexpr NullInt32 a{42};
    constexpr NullInt32 b{42};
    constexpr NullInt32 c{99};
    ASSERT_EQ(a, b);
    ASSERT_NE(a, c);
}

TEST(OptionalInt, NullEqualsNull)
{
    constexpr NullInt32 a;
    constexpr NullInt32 b;
    ASSERT_EQ(a, b);
}

TEST(OptionalInt, NullNotEqualToValue)
{
    constexpr NullInt32 a;
    constexpr NullInt32 b{0};
    ASSERT_NE(a, b);
}

TEST(OptionalInt, Ordering)
{
    constexpr NullInt32 a{10};
    constexpr NullInt32 b{20};
    ASSERT_LT(a, b);
    ASSERT_LE(a, b);
    ASSERT_GT(b, a);
    ASSERT_GE(b, a);
    ASSERT_LE(a, a);
    ASSERT_GE(a, a);
}

TEST(OptionalInt, NullSortsBeforeValue)
{
    constexpr NullInt32 n;
    constexpr NullInt32 v{0};
    ASSERT_LT(n, v);
    ASSERT_FALSE(v < n);
    ASSERT_FALSE(n < n);
}

// --- Unsigned type ---

TEST(OptionalInt, UnsignedArithmetic)
{
    constexpr NullUint32 a{100};
    constexpr NullUint32 b{30};
    ASSERT_EQ(130u, (a + b).value());
    ASSERT_EQ(70u, (a - b).value());
    ASSERT_EQ(3000u, (a * b).value());
    ASSERT_EQ(3u, (a / b).value());
    ASSERT_EQ(10u, (a % b).value());
}

// --- Stream operator ---

TEST(OptionalInt, StreamValue)
{
    std::ostringstream os;
    os << NullInt32{42};
    ASSERT_EQ("42", os.str());
}

TEST(OptionalInt, StreamNull)
{
    std::ostringstream os;
    os << NullInt32::null();
    ASSERT_EQ("null", os.str());
}

// --- Implicit conversion from T ---

TEST(OptionalInt, ImplicitConversionFromLiteral)
{
    NullInt32 value = 42;
    ASSERT_TRUE(value.hasValue());
    ASSERT_EQ(42, value.value());
}

TEST(OptionalInt, MixedArithmeticWithLiteral)
{
    constexpr NullInt32 a{10};
    constexpr auto result = a + NullInt32{5};
    ASSERT_EQ(15, result.value());
}

// --- Constexpr verification ---

TEST(OptionalInt, ConstexprArithmetic)
{
    static_assert((NullInt32{10} + NullInt32{20}).value() == 30);
    static_assert((NullInt32{10} * NullInt32{3}).value() == 30);
    static_assert(!(NullInt32{} + NullInt32{1}).hasValue());
    static_assert(NullInt32{}.valueOr(99) == 99);
}

}