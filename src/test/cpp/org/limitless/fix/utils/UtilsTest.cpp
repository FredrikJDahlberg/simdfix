//
// Created by Fredrik Dahlberg on 2026-04-27.
//

#include <array>
#include <string_view>
#include <chrono>

#include <gtest/gtest.h>

#include "org/limitless/fix/utils/Utils.hpp"
#include "org/limitless/fix/unused/QuadSearch.hpp"

namespace org::limitless::fix::decoder {

TEST(QuadSearch, Sorted)
{
    {
        const uint16_t values[] = {1, 2, 5, 7, 9, 12, 14, 15, 18, 23, 24, 29, 33, 35, 37, 40, 42 };
        ASSERT_EQ(-1, simd::quadSearch(values, std::size(values), 47));
        ASSERT_EQ(0, simd::quadSearch(values, std::size(values), 1));
        ASSERT_EQ(16, simd::quadSearch(values, std::size(values), 42));
    }
    {
        const uint16_t values[] = { 8, 9, 49, 35, 56, 34, 52, 1128, 98 };
        ASSERT_EQ(5, simd::quadSearch(values, std::size(values), 34));
        ASSERT_EQ(3, simd::quadSearch(values, std::size(values), 35));
        ASSERT_EQ(8, simd::quadSearch(values, std::size(values), 98));
    }
    {
        const uint16_t values[] = { 0, 0, 0, 0, 0, 0, 0, 0, 8, 9, 34, 35, 49, 52, 56, 98, 1128 };
        ASSERT_EQ(10, simd::quadSearch(values, std::size(values), 34));
        ASSERT_EQ(11, simd::quadSearch(values, std::size(values), 35));
        ASSERT_EQ(16, simd::quadSearch(values, std::size(values), 1128));
    }
}

TEST(Parse, AsciiToDecimal)
{
    // asciiToUint64 reads 8 bytes regardless of length, so inputs need padding.
    {
        constexpr uint8_t value[] = { '1', '2', '3', '4', 0, 0, 0, 0 };
        ASSERT_EQ(1234ULL, utils::asciiToUint64(0, value, 4, true));
    }
    {
        constexpr uint8_t value[] = { '3', '4', '5', 0, 0, 0, 0, 0 };
        ASSERT_EQ(12345ULL, utils::asciiToUint64(12, value, 3, true));
    }
    {
        const auto value = reinterpret_cast<const uint8_t*>("12345678901234");
        ASSERT_EQ(1234ULL, utils::asciiToUint64(0, value, 4, true));
    }
}

TEST(Parse, BinaryToDecimal)
{
    // binaryToDecimal reads 8 bytes regardless of length, so inputs need padding.
    {
        constexpr uint8_t value[]= { 1, 2, 3, 4, 0, 0, 0, 0 };
        ASSERT_EQ(1234ULL, utils::asciiToUin32(0, value, 4));
    }
    {
        constexpr uint8_t value[]= { 3, 4, 5, 0, 0, 0, 0, 0 };
        ASSERT_EQ(12345ULL, utils::asciiToUin32(12, value, 3));
    }
}

TEST(Parse, AsciiToInt32)
{
    const auto check = [](const std::string_view text, const int32_t expected)
    {
        ASSERT_EQ(expected, utils::asciiToInt32(reinterpret_cast<const uint8_t*>(text.data()), text.size())) << "text=" << text;
    };

    check("0\0\0\0\0\0\0\0", 0);
    check("1\0\0\0\0\0\0\0", 1);
    check("9\0\0\0\0\0\0\0", 9);
    check("10\0\0\0\0\0\0", 10);
    check("12345678", 12345678);
    check("2147483647", 2147483647);
    check("-1\0\0\0\0\0\0", -1);
    check("-9\0\0\0\0\0\0", -9);
    check("-10\0\0\0\0\0", -10);
    check("-12345678", -12345678);
    check("-2147483648", -2147483648);
}

TEST(Time, DateTimeToEpochUTC)
{
    // epoch reference point
    EXPECT_EQ(0LL,             utils::dateTimeToEpochUTC("19700101-00:00:00.000").count());
    EXPECT_EQ(1767225600000LL, utils::dateTimeToEpochUTC("20260101-00:00:00.000").count());
    EXPECT_EQ(1767269594000LL, utils::dateTimeToEpochUTC("20260101-12:13:14.000").count());
    EXPECT_EQ(1780835696000LL, utils::dateTimeToEpochUTC("20260607-12:34:56").count());
    EXPECT_EQ(1780835696123LL, utils::dateTimeToEpochUTC("20260607-12:34:56.123").count());
    EXPECT_EQ(1781515800500LL, utils::dateTimeToEpochUTC("20260615-09:30:00.500").count());

    // leap day and century leap year (2000 is divisible by 400)
    EXPECT_EQ(951825600000LL,  utils::dateTimeToEpochUTC("20000229-12:00:00.000").count());
    EXPECT_EQ(946684800000LL,  utils::dateTimeToEpochUTC("20000101-00:00:00.000").count());

    // day after Feb 29th differs between a leap year (2020) and a non-leap year (2021)
    EXPECT_EQ(1582934400000LL, utils::dateTimeToEpochUTC("20200229-00:00:00.000").count());
    EXPECT_EQ(1583020800000LL, utils::dateTimeToEpochUTC("20200301-00:00:00.000").count());
    EXPECT_EQ(1614556800000LL, utils::dateTimeToEpochUTC("20210301-00:00:00.000").count());

    // year boundary spanning into a leap year
    EXPECT_EQ(1704067199999LL, utils::dateTimeToEpochUTC("20231231-23:59:59.999").count());
    EXPECT_EQ(1704067200000LL, utils::dateTimeToEpochUTC("20240101-00:00:00.000").count());

    // far future date
    EXPECT_EQ(4102444799999LL, utils::dateTimeToEpochUTC("20991231-23:59:59.999").count());
}

TEST(Time, DateTimeToEpochUTCInvalidLength)
{
    EXPECT_EQ(-1, utils::dateTimeToEpochUTC("").count());
    EXPECT_EQ(-1, utils::dateTimeToEpochUTC("20260607-12:34:56.12").count()); // one char short, 20 chars
}

TEST(Encode, Uint32ToAscii)
{
    const auto check = [](const uint32_t value, const std::string_view expected)
    {
        std::array<uint8_t, 16> buffer{};
        const auto length = utils::uint32ToAscii(value, buffer, 0);
        ASSERT_EQ(expected.size(), length) << "value=" << value << " expected = " << expected;
        EXPECT_EQ(expected, (std::string_view{reinterpret_cast<const char*>(buffer.data()), length})) << "value=" << value;
    };

    check(0, "0");
    check(1, "1");
    check(9, "9");
    check(10, "10");
    check(99, "99");
    check(100, "100");
    check(999, "999");
    check(1000, "1000");
    check(9999, "9999");
    check(10000, "10000");
    check(99999, "99999");
    check(100000, "100000");
    check(123456789, "123456789");
    check(4294967294, "4294967294");
    check(4294967295, "4294967295");
}

TEST(Encode, Uint32ToAsciiOffset)
{
    std::array<uint8_t, 16> buffer{};
    buffer[0] = 'X';
    const auto length = utils::uint32ToAscii(42, buffer, 1);
    ASSERT_EQ(2U, length);
    EXPECT_EQ((std::string_view{reinterpret_cast<const char*>(buffer.data()), 3}), "X42");
}

TEST(Encode, Int32ToAscii)
{
    const auto check = [](const int32_t value, const std::string_view expected)
    {
        std::array<uint8_t, 16> buffer{};
        const auto length = utils::int32ToAscii(value, buffer, 0);
        ASSERT_EQ(expected.size(), length) << "value=" << value << " expected = " << expected;
        EXPECT_EQ(expected, (std::string_view{reinterpret_cast<const char*>(buffer.data()), length})) << "value=" << value;
    };

    check(0, "0");
    check(1, "1");
    check(9, "9");
    check(10, "10");
    check(99999, "99999");
    check(100000, "100000");
    check(2147483647, "2147483647");
    check(-1, "-1");
    check(-9, "-9");
    check(-10, "-10");
    check(-99999, "-99999");
    check(-100000, "-100000");
    check(-2147483647, "-2147483647");
    check(-2147483648, "-2147483648");
}

TEST(Encode, Int32ToAsciiOffset)
{
    std::array<uint8_t, 16> buffer{};
    buffer[0] = 'X';
    const auto length = utils::int32ToAscii(-42, buffer, 1);
    ASSERT_EQ(3U, length);
    EXPECT_EQ((std::string_view{reinterpret_cast<const char*>(buffer.data()), 4}), "X-42");
}

TEST(Encode, Uint64ToAscii)
{
    const auto check = [](const uint64_t value, const std::string_view expected)
    {
        std::array<uint8_t, 24> buffer{};
        const auto length = utils::uint64ToAscii(value, buffer, 0);
        ASSERT_EQ(expected.size(), length) << "value=" << value << " expected = " << expected;
        EXPECT_EQ(expected, (std::string_view{reinterpret_cast<const char*>(buffer.data()), length})) << "value=" << value;
    };

    check(0, "0");
    check(1, "1");
    check(9, "9");
    check(10, "10");
    check(99, "99");
    check(100, "100");
    check(999, "999");
    check(9999, "9999");
    check(99999, "99999");
    check(123456789, "123456789");
    check(4294967294, "4294967294");
    check(4294967295, "4294967295");
    check(4294967296, "4294967296");
    check(999999999999999999ULL, "999999999999999999");
    check(1000000000000000000ULL, "1000000000000000000");
    check(18446744073709551614ULL, "18446744073709551614");
    check(18446744073709551615ULL, "18446744073709551615");
}

TEST(Encode, Uint64ToAsciiOffset)
{
    std::array<uint8_t, 24> buffer{};
    buffer[0] = 'X';
    const auto length = utils::uint64ToAscii(42, buffer, 1);
    ASSERT_EQ(2U, length);
    EXPECT_EQ((std::string_view{reinterpret_cast<const char*>(buffer.data()), 3}), "X42");
}

TEST(Encode, Int64ToAscii)
{
    const auto check = [](const int64_t value, const std::string_view expected)
    {
        std::array<uint8_t, 24> buffer{};
        const auto length = utils::int64ToAscii(value, buffer, 0);
        ASSERT_EQ(expected.size(), length) << "value=" << value << " expected = " << expected;
        EXPECT_EQ(expected, (std::string_view{reinterpret_cast<const char*>(buffer.data()), length})) << "value=" << value;
    };

    check(0, "0");
    check(1, "1");
    check(9, "9");
    check(10, "10");
    check(99999, "99999");
    check(100000, "100000");
    check(9223372036854775807LL, "9223372036854775807");
    check(-1, "-1");
    check(-9, "-9");
    check(-10, "-10");
    check(-99999, "-99999");
    check(-100000, "-100000");
    check(-9223372036854775807LL, "-9223372036854775807");
    check(-9223372036854775807LL - 1, "-9223372036854775808");
}

TEST(Encode, Int64ToAsciiOffset)
{
    std::array<uint8_t, 24> buffer{};
    buffer[0] = 'X';
    const auto length = utils::int64ToAscii(-42, buffer, 1);
    ASSERT_EQ(3U, length);
    EXPECT_EQ((std::string_view{reinterpret_cast<const char*>(buffer.data()), 4}), "X-42");
}

TEST(Encode, FixedDecimalToAscii)
{
    const auto check = [](const int64_t mantissa, const int32_t exponent, const std::string_view expected)
    {
        std::array<uint8_t, 24> buffer{};
        const auto length = utils::fixedDecimalToAscii(mantissa, exponent, buffer, 0);
        ASSERT_EQ(expected.size(), length) << "mantissa=" << mantissa << " exponent=" << exponent;
        EXPECT_EQ(expected, (std::string_view{reinterpret_cast<const char*>(buffer.data()), length}))
            << "mantissa=" << mantissa << " exponent=" << exponent;
    };

    check(0, 0, "0");
    check(0, -2, "0");
    check(123, 0, "123");
    check(123, 2, "12300");
    check(12345, -2, "123.45");
    check(1, -3, "0.001");
    check(100, -2, "1.00");
    check(-12345, -2, "-123.45");
    check(-1, -3, "-0.001");
    check(-100, -2, "-1.00");
}

}
