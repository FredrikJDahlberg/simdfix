//
// Created by Fredrik Dahlberg on 2026-04-27.
//

#include <iostream>
#include <string_view>
#include <chrono>

#include <gtest/gtest.h>

#include "org/limitless/fix/utils/Utils.hpp"
#include "org/limitless/fix/simd/QuadSearch.hpp"
#include "org/limitless/fix/simd/LinearSearch.hpp"

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
        const uint16_t values[] = { 0, 0, 0, 0, 0, 0, 0, 0, 8, 9, 49, 35, 56, 34, 52, 1128, 98 };
        ASSERT_EQ(13, simd::quadSearch(values, std::size(values), 34));
        ASSERT_EQ(11, simd::quadSearch(values, std::size(values), 35));
        ASSERT_EQ(16, simd::quadSearch(values, std::size(values), 98));
    }
}

TEST(Find, Basics)
{
    {
        constexpr uint16_t values[] = {  1, 49, 4711, 10  };
        ASSERT_EQ(2, simd::find(values, std::size(values), 4711));
        ASSERT_EQ(3, simd::find(values, std::size(values), 10));
    }
    {
        const uint16_t values[] = {
            31, 32, 33, 34, 35, 36,37, 38, 39,
            20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
            11, 12, 13, 14, 15, 16, 17, 18, 19,
            1, 2, 3, 4, 5, 6, 7, 8, 9, 10
        };
        ASSERT_EQ(37, simd::find(values, std::size(values), 10));
    }
}

TEST(Parse, AsciiToDecimal)
{
    {
        const auto value = reinterpret_cast<const uint8_t*>("1234");
        ASSERT_EQ(1234, utils::asciiToDecimal(0, value, 4));
    }
    {
        const auto value = reinterpret_cast<const uint8_t*>("345");
        ASSERT_EQ(12345, utils::asciiToDecimal(12, value, 3));
    }
    {
        const auto value = reinterpret_cast<const uint8_t*>("12345678901234");
        ASSERT_EQ(1234, utils::asciiToDecimal(0, value, 4));
    }
}

TEST(Parse, BinaryToDecimal)
{
    // binaryToDecimal reads 8 bytes regardless of length, so inputs need padding.
    {
        constexpr uint8_t value[]= { 1, 2, 3, 4, 0, 0, 0, 0 };
        ASSERT_EQ(1234, utils::binaryToDecimal(0, value, 4));
    }
    {
        constexpr uint8_t value[]= { 3, 4, 5, 0, 0, 0, 0, 0 };
        ASSERT_EQ(12345, utils::binaryToDecimal(12, value, 3));
    }
}

TEST(Time, DateTimeToEpochUTC)
{
    // epoch reference point
    EXPECT_EQ(0LL,             utils::dateTimeToEpochUTC("19700101-00:00:00.000"));
    EXPECT_EQ(1767225600000LL, utils::dateTimeToEpochUTC("20260101-00:00:00.000"));
    EXPECT_EQ(1767269594000LL, utils::dateTimeToEpochUTC("20260101-12:13:14.000"));
    EXPECT_EQ(1780835696000LL, utils::dateTimeToEpochUTC("20260607-12:34:56"));
    EXPECT_EQ(1780835696123LL, utils::dateTimeToEpochUTC("20260607-12:34:56.123"));
    EXPECT_EQ(1781515800500LL, utils::dateTimeToEpochUTC("20260615-09:30:00.500"));

    // leap day and century leap year (2000 is divisible by 400)
    EXPECT_EQ(951825600000LL,  utils::dateTimeToEpochUTC("20000229-12:00:00.000"));
    EXPECT_EQ(946684800000LL,  utils::dateTimeToEpochUTC("20000101-00:00:00.000"));

    // day after Feb 29th differs between a leap year (2020) and a non-leap year (2021)
    EXPECT_EQ(1582934400000LL, utils::dateTimeToEpochUTC("20200229-00:00:00.000"));
    EXPECT_EQ(1583020800000LL, utils::dateTimeToEpochUTC("20200301-00:00:00.000"));
    EXPECT_EQ(1614556800000LL, utils::dateTimeToEpochUTC("20210301-00:00:00.000"));

    // year boundary spanning into a leap year
    EXPECT_EQ(1704067199999LL, utils::dateTimeToEpochUTC("20231231-23:59:59.999"));
    EXPECT_EQ(1704067200000LL, utils::dateTimeToEpochUTC("20240101-00:00:00.000"));

    // far future date
    EXPECT_EQ(4102444799999LL, utils::dateTimeToEpochUTC("20991231-23:59:59.999"));
}

TEST(Time, DateTimeToEpochUTCInvalidLength)
{
    EXPECT_EQ(-1, utils::dateTimeToEpochUTC(""));
    EXPECT_EQ(-1, utils::dateTimeToEpochUTC("20260607-12:34:56.12")); // one char short, 20 chars
}

}
