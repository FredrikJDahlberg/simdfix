//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#include <gtest/gtest.h>

#include "org/limitless/fix/parser/Tokenizer.hpp"

namespace org::limitless::fix::parser {

#define SOH "\x01"
static constexpr uint8_t MESSAGE1[] =
    "8=FIXT.1.1" SOH
    "9=116" SOH
    "35=A" SOH
    "49=Buyer" SOH
    "56=SellSide_1" SOH
    "34=1" SOH
    "52=20190605-11:51:27.848" SOH
    "1128=9" SOH
    "98=0" SOH
    "108=30" SOH
    "141=Y" SOH
    "553=Username" SOH
    "554=Password" SOH
    "1137=9" SOH
    "10=079" SOH
    // next message
    "8=FIXT.1.1" SOH
    "          ";

TEST(Tokenizer, Basics)
{
    Tokenizer tokenizer;
    const auto start = std::chrono::high_resolution_clock::now();
    tokenizer.scan(MESSAGE1, sizeof(MESSAGE1) - 1);
    const auto end = std::chrono::high_resolution_clock::now();
    const auto  duration = std::chrono::nanoseconds(end - start);
    std::printf("%8lld\n", duration.count());

    // FIXME: tag 141
    // FIXME tag 1137
    // tokenizer.forEach([](const Tokenizer::Token& token) {
//        std::printf("tag = %3d, pos = %d, len = %d\n", token.tag, token.valueOffset, token.valueLength);
  //  });

    const Tokenizer::Token expectedTokens[] =
    {
        { 8, 2, 8 },
        { 9, 13, 3 },
        { 35, 20, 1 },
        { 49, 25, 5 },
        { 56, 34, 10 },
        { 34, 48, 1 },
        { 52, 53, 21 },
        { 1128, 80, 1 },
        { 98, 85, 1 },
        { 108, 91, 2 },
        { 141, 96, 1 },
        { 553, 0, 8 },
        { 554, 0, 8 },
        { 1137, 0, 1 },
        { 10, 0, 3 },
        { 0, 0, 0 }
    };
    int i = 0;
    for (auto [tag, valueOffset, valueLength] : tokenizer)
    {
        std::printf("tag = %d, pos = %d, len = %d\n", tag, valueOffset, valueLength);
        const Tokenizer::Token& expected = expectedTokens[i];
        ++i;
        ASSERT_EQ(expected.tag, tag) << "Invalid tag value " << tag;
        ASSERT_EQ(expected.valueOffset, valueOffset) << "Tag " << expected.tag << " has invalid offset";
        ASSERT_EQ(expected.valueLength, valueLength) << "Tag " << expected.tag << " has invalid length";
    }
}
}
