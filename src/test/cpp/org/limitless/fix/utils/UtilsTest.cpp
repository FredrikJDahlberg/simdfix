//
// Created by Fredrik Dahlberg on 2026-04-27.
//

#include <gtest/gtest.h>

#include "org/limitless/fix/utils/Utils.hpp"
#include "org/limitless/fix/simd/QuadSearch.hpp"
#include "org/limitless/fix/simd/LinearSearch.hpp"

namespace org::limitless::fix::decoder {

TEST(QuadSearch, Sorted)
{
    const uint16_t values[] = {1, 2, 5, 7, 9, 12, 14, 15, 18, 23, 24, 29, 33, 35, 37, 40, 42 };
    ASSERT_EQ(-1, simd::quadSearch(values, std::size(values), 47));
    ASSERT_EQ(0, simd::quadSearch(values, std::size(values), 1));
    ASSERT_EQ(16, simd::quadSearch(values, std::size(values), 42));
}

TEST(QuadSearch, Unsorted)
{
    const uint16_t values[] = { 8, 9, 49, 35, 56, 34, 52, 1128, 98 };
    ASSERT_EQ(5, simd::quadSearch(values, std::size(values), 34));
    ASSERT_EQ(3, simd::quadSearch(values, std::size(values), 35));
    ASSERT_EQ(8, simd::quadSearch(values, std::size(values), 98));
}

TEST(QuadSearch, UnsortedLarger)
{
    const uint16_t values[] = { 0, 0, 0, 0, 0, 0, 0, 0, 8, 9, 49, 35, 56, 34, 52, 1128, 98 };
    ASSERT_EQ(13, simd::quadSearch(values, std::size(values), 34));
    ASSERT_EQ(11, simd::quadSearch(values, std::size(values), 35));
    ASSERT_EQ(16, simd::quadSearch(values, std::size(values), 98));
}

TEST(Find, Basics)
{
    {
        const uint16_t values[] = {  1, 49, 4711, 10  };
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
}

TEST(Parse, BinaryToDecimal)
{
    {
        constexpr uint8_t value[]= { 1, 2, 3, 4 };
        ASSERT_EQ(1234, utils::binaryToDecimal(0, value, 4));
    }
    {
        constexpr uint8_t value[]= { 3, 4, 5 };
        ASSERT_EQ(12345, utils::binaryToDecimal(12, value, 3));
    }
}
}
