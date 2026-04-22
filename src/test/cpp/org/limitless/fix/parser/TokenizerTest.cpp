//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#include <gtest/gtest.h>

#include "org/limitless/fix/parser/Tokenizer.hpp"

namespace org::limitless::fix::parser {

#define SOH "\x01"
// body = 5 + 9 + 14 + 5 + 27 + 7 + 5 + 7 + 6 + 13 + 13 + 7 = 118
static constexpr uint8_t MESSAGE1[] =
    "8=FIXT.1.1" SOH
    "9=118" SOH
    "35=A" SOH // 5
    "49=Buyer" SOH // 9
    "56=SellSide_1" SOH // 14
    "34=1" SOH // 5
    "52=20190605-11:51:27.84800" SOH // 27
    "1128=9" SOH // 7
    "98=0" SOH // 5
    "108=30" SOH // 7
    "141=Y" SOH // 6
    "553=Username" SOH // 13
    "554=Password" SOH // 13
    "1137=9" SOH // 7
    "10=147" SOH
    // next message
    "                   ";  // padding
TEST(Tokenizer, Basics)
{
    Tokenizer tokenizer;
    tokenizer.scan(MESSAGE1, sizeof(MESSAGE1) - 1);

    const Tokenizer::Token expectedTokens[] =
    {
        { 2, 8, 8 },
        { 13, 9, 3 },
        { 20, 35, 1 },
        { 25, 49, 5 },
        { 34, 56, 10 },
        { 48, 34, 1 },
        { 53, 52, 23 },
        { 82, 1128, 1 },
        { 87, 98, 1 },
        { 93, 108, 2 },
        { 100, 141, 1 },
        { 106, 553, 8 },
        { 119, 554, 8 },
        { 133, 1137, 1 },
        { 138, 10, 3 },
    };
    int i = 0;
    for (auto [valueOffset, tag, valueLength] : tokenizer)
    {
        const auto& expected = expectedTokens[i];
        std::printf("%3d, tag = %d, pos = %d, len = %d\n", i, tag, valueOffset, valueLength);
        ++i;
        ASSERT_EQ(expected.tag, tag) << "Invalid tag value " << tag;
        ASSERT_EQ(expected.position, valueOffset) << "Tag " << expected.tag << " has invalid offset";
        ASSERT_EQ(expected.length, valueLength) << "Tag " << expected.tag << " has invalid length";
    }
}
}
