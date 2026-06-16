//
// Created by Fredrik Dahlberg on 2026-06-16.
//

#include <array>

#include <gtest/gtest.h>
#include "org/limitless/fix/simd/LinearSearch.hpp"

using namespace org::limitless::fix::simd;

TEST(LinearSearch, EmptyArray)
{
    uint16_t array[] = {1};
    EXPECT_EQ(find(array, 0, 1), -1);
}

TEST(LinearSearch, SingleElement)
{
    uint16_t array[] = {42};
    EXPECT_EQ(find(array, 1, 42),  0);
    EXPECT_EQ(find(array, 1, 99), -1);
}

// cardinality < 8: only the scalar tail loop runs
TEST(LinearSearch, ScalarPath)
{
    uint16_t array[] = {10, 20, 30, 40, 50, 60, 70};
    constexpr int32_t n = 7;
    for (int32_t i = 0; i < n; ++i)
        EXPECT_EQ(find(array, n, array[i]), i) << "index " << i;
    EXPECT_EQ(find(array, n, 99), -1);
}

// cardinality == 8: SIMD loop fires once, scalar tail is empty
TEST(LinearSearch, SimdExact8)
{
    uint16_t array[] = {10, 20, 30, 40, 50, 60, 70, 80};
    constexpr int32_t n = 8;
    for (int32_t i = 0; i < n; ++i)
        EXPECT_EQ(find(array, n, array[i]), i) << "index " << i;
    EXPECT_EQ(find(array, n, 99), -1);
}

// cardinality == 9: SIMD loop fires once, scalar tail handles index 8
TEST(LinearSearch, SimdPlusScalarTail)
{
    uint16_t array[] = {10, 20, 30, 40, 50, 60, 70, 80, 90};
    constexpr int32_t n = 9;
    for (int32_t i = 0; i < n; ++i)
        EXPECT_EQ(find(array, n, array[i]), i) << "index " << i;
    EXPECT_EQ(find(array, n, 99), -1);
}

// cardinality == 16: SIMD loop fires twice, no scalar tail
TEST(LinearSearch, TwoSimdChunks)
{
    uint16_t array[16];
    for (int32_t i = 0; i < 16; ++i)
        array[i] = static_cast<uint16_t>((i + 1) * 10);
    constexpr int32_t n = 16;
    for (int32_t i = 0; i < n; ++i)
        EXPECT_EQ(find(array, n, array[i]), i) << "index " << i;
    EXPECT_EQ(find(array, n, 999), -1);
}

// duplicate values: first occurrence must be returned
TEST(LinearSearch, DuplicatesReturnFirst)
{
    uint16_t array[] = {10, 20, 10, 30, 10};
    EXPECT_EQ(find(array, 5, 10), 0);
    EXPECT_EQ(find(array, 5, 20), 1);
    EXPECT_EQ(find(array, 5, 30), 3);
}

TEST(LinearSearch, Basics)
{
    {
        constexpr uint16_t values[] = {1, 49, 4711, 10};
        ASSERT_EQ(2, find(values, std::size(values), 4711));
        ASSERT_EQ(3, find(values, std::size(values), 10));
    }
    {
        const uint16_t values[] = {
            31, 32, 33, 34, 35, 36, 37, 38, 39,
            20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
            11, 12, 13, 14, 15, 16, 17, 18, 19,
            1, 2, 3, 4, 5, 6, 7, 8, 9, 10
        };
        ASSERT_EQ(37, find(values, std::size(values), 10));
    }
}