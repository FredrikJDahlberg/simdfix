//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#include <gtest/gtest.h>

#include "org/limitless/fix/parser/Tokenizer.hpp"

namespace org::limitless::fix::parser {

#define SOH "\x01"

void check(std::span<const Tokenizer::Token> result, const std::span<const Tokenizer::Token> expected)
{
    for (int i = 0; auto& [position, tag, length] : result)
    {
        const auto& token = expected[i++];
        std::printf("%3d, tag = %4d, pos = %4d, len = %4d\n", i, tag, position, length);
        ASSERT_EQ(token.tag, tag) << "Mismatch at index " << i - 1;
        ASSERT_EQ(token.position, position) << "Tag " << token.tag << " has invalid offset";
        ASSERT_EQ(token.length, length) << "Tag " << token.tag << " has invalid length";
    }
    ASSERT_EQ(expected.size(), result.size());
}

TEST(Tokenizer, Basics)
{
    static constexpr uint8_t MESSAGE[] =
        "8=FIXT.1.1" SOH "9=118" SOH "35=A" SOH "49=Buyer" SOH "56=SellerSide" SOH "34=1" SOH
        "52=20190605-11:51:27.84800" SOH "1128=9" SOH "98=0" SOH "108=30" SOH "141=Y" SOH "553=Username" SOH
        "554=Password" SOH "1137=9" SOH "10=218" SOH
        // next message
        "8=FIXT.1.1" SOH "9=118" SOH;
    constexpr size_t LENGTH = sizeof(MESSAGE) - 1;
    constexpr std::span buffer(MESSAGE, LENGTH);
    Tokenizer tokenizer;
    auto [processed, checkSum] = tokenizer.scan(buffer);
    ASSERT_EQ(LENGTH - 17, processed);
    ASSERT_EQ(218, checkSum);
    constexpr Tokenizer::Token expectedTokens[] =
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
    check(tokenizer.tokens(), std::span(expectedTokens, std::size(expectedTokens)));
}

TEST(Tokenizer, SplitTagLast)
{
    static constexpr uint8_t MESSAGE[] = "8=FIXT.1.1" SOH "9=49" SOH "35=A" SOH "49=Buyer" SOH "56=SellSide" SOH "10=147" SOH;
    constexpr size_t LENGTH = sizeof(MESSAGE) - 1;
    constexpr std::span buffer(MESSAGE, LENGTH);
    Tokenizer tokenizer{};
    auto [processed, checkSum] = tokenizer.scan(buffer);
    ASSERT_EQ(LENGTH, processed);
    ASSERT_EQ(170, checkSum);
    constexpr Tokenizer::Token expectedTokens[] =
    {
        { 2, 8, 8 },
        { 13, 9, 2 },
        { 19, 35, 1 },
        { 24, 49, 5 },
        { 33, 56, 8 },
        { 45, 10, 3 }
    };
    check(tokenizer.tokens(), std::span(expectedTokens, std::size(expectedTokens)));
}

}
