//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#include <gtest/gtest.h>

#include "org/limitless/fix/parser/Tokenizer.hpp"

namespace org::limitless::fix::parser {

void verify(Tokenizer::Token* tokens, size_t count, Tokenizer::Token* expected, Tokenizer::Token* actual)
{

}

#define SOH "\x01"
// body = 5 + 9 + 14 + 5 + 27 + 7 + 5 + 7 + 6 + 13 + 13 + 7 = 118
static uint8_t MESSAGE1[] =
    "8=FIXT.1.1" SOH // 11
    "9=118" SOH // 6
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
    "10=147" SOH // 7
    // next message
    "8=FIXT.1.1" SOH
    "9=118" SOH;

TEST(Tokenizer, Basics)
{
    Tokenizer tokenizer;
    const auto processed = tokenizer.scan(MESSAGE1, sizeof(MESSAGE1) - 1);
    ASSERT_EQ(142, processed) << "Invalid message length = " << processed;

    const Tokenizer::Token expectedTokens[] =
    {
        // { 2, 8, 8 },
        // { 13, 9, 3 },
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
        // { 138, 10, 3 },
    };

    for (int i = 0; auto& [position, tag, length] : tokenizer.tokens())
    {
        const auto& expected = expectedTokens[i++];
        std::printf("%3d, tag = %4d, pos = %4d, len = %4d\n", i, tag, position, length);
        ASSERT_EQ(expected.tag, tag) << "Mismatch at index " << i - 1;
        ASSERT_EQ(expected.position, position) << "Tag " << expected.tag << " has invalid offset";
        ASSERT_EQ(expected.length, length) << "Tag " << expected.tag << " has invalid length";
    }
}
}
