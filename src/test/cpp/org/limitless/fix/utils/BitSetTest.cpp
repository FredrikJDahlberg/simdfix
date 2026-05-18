//
// Created by Fredrik Dahlberg on 2026-04-24.
//
#include <gtest/gtest.h>

#include "../../../../../../main/cpp/org/limitless/fix/utils/BitSet64.hpp"

namespace org::limitless::fix::decoder {

TEST(BitSet, Basics)
{
    using namespace org::limitless::fix::decoder;

    BitSet64 bits{};
    bits.set();
    ASSERT_EQ(0, bits.zerosRight());
    bits.clear();
    ASSERT_EQ(64, bits.zerosRight());

    bits.set();
    bits >>= 64 - 3;
    ASSERT_EQ(0, bits.zerosRight());
    bits.clear(0);
    ASSERT_EQ(1, bits.zerosRight());
    bits.clear(1);
    ASSERT_EQ(2, bits.zerosRight());
    bits.clear(2);
    ASSERT_EQ(64, bits.zerosRight());
}
}
