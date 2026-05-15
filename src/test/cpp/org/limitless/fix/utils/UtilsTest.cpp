//
// Created by Fredrik Dahlberg on 2026-04-27.
//

#include <gtest/gtest.h>

#include "org/limitless/fix/utils/Utils.hpp"
#include "org/limitless/fix/simd/QuadSearch.hpp"

namespace org::limitless::fix::parser {
struct Meta
{
    int32_t tag;
    int32_t type;
    bool required;
};

TEST(QuadSearch, Basics)
{
    const uint16_t values[] = {1, 2, 5, 7, 9, 12, 14, 15, 18, 23, 24, 29, 33, 35, 37, 40, 42 };
    ASSERT_EQ(-1, simd::quadSearch(values, std::size(values), 47));
    ASSERT_EQ(0, simd::quadSearch(values, std::size(values), 1));
    ASSERT_EQ(16, simd::quadSearch(values, std::size(values), 42));
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
