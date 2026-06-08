//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#include <gtest/gtest.h>

#include "org/limitless/fix/utils/Utils.hpp"
#include "org/limitless/fix/decoder/PayloadDecoder.hpp"

namespace org::limitless::fix::decoder {

#define SOH "\x01"

void check(std::span<Token> result, const std::span<const Token> expected)
{
    for (int i = 0; auto& [position, tag, length] : result)
    {
        const auto [expectedPos, expectedTag, expectedLen ] = expected[i++];
#if !defined(NDEBUG)
        std::printf("%3d, tag = %4d, pos = %4d, len = %4d\n", i, tag, position, length);
#endif
        ASSERT_EQ(expectedTag, tag) << "Mismatch at index " << i - 1;
        ASSERT_EQ(expectedPos, position) << "Tag " << expectedTag << " has invalid offset";
        ASSERT_EQ(expectedLen, length) << "Tag " << expectedTag << " has invalid length";
    }
    ASSERT_EQ(expected.size(), result.size());
}

TEST(Decoder, Basics)
{
    const auto message = utils::makeSpan("8=FIXT.1.1" SOH "9=118" SOH "35=A" SOH "49=Buyer" SOH
        "56=SellerSide" SOH "34=1" SOH "52=20190605-11:51:27.84800" SOH "1128=9" SOH "98=0" SOH "108=30" SOH
        "141=Y" SOH "553=Username" SOH "554=Password" SOH "1137=9" SOH "10=218" SOH
        // next message
        "8=FIXT.1.1" SOH "9=118" SOH);
    PayloadDecoder decoder;
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::Success, status);
    ASSERT_EQ(message.size() - 17, processed);
    // ASSERT_EQ(218, checkSum);
    constexpr Token expectedTokens[] =
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
    check(decoder.tokens(), std::span(expectedTokens, std::size(expectedTokens)));
}

TEST(Decoder, TrailerSplitCheckSum)
{
    const auto message = utils::makeSpan("8=FIXT.1.1" SOH "9=47" SOH "35=A" SOH
        "49=Buyer" SOH "56=Seller" SOH "34=2000001" SOH "52=20190605" SOH "10=046" SOH);
    PayloadDecoder decoder{};
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::Success, status);
    ASSERT_EQ(message.size(), processed);
    // ASSERT_EQ(48, checkSum);

    constexpr Token expectedTokens[] =
    {
        { 2, 8, 8 },
        { 13, 9, 2 },
        { 19, 35, 1 },
        { 24, 49, 5 },
        { 33, 56, 6 },
        { 43, 34, 7 },
        { 54, 52, 8 },
        { 66, 10, 3 }
    };
    check(decoder.tokens(), std::span(expectedTokens, std::size(expectedTokens)));
}

TEST(Decoder, TrailerFieldEnd)
{
    const auto message = utils::makeSpan("8=FIXT.1.1" SOH "9=21" SOH "35=66" SOH
        "666=66" SOH "1=1" SOH "2=2" SOH "10=233" SOH);
    PayloadDecoder decoder{};
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::Success, status);
}

TEST(Decoder, Fragment)
{
    const auto message = utils::makeSpan("8=FIXT.");
    PayloadDecoder decoder{};
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::MessageFragment, status);
    ASSERT_EQ(0, processed);
}

TEST(Decoder, HopGroup)
{
    const auto logout = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=84" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=10:11:12.123" SOH
        "627=2" SOH "629=10" SOH "628=12" SOH "629=37" SOH "628=20" SOH "10=211" SOH);
    PayloadDecoder decoder{};
    auto [processed, status] = decoder.parse(logout);
    ASSERT_EQ(Result::Success, status);
    constexpr Token expectedTokens[] =
    {
        { 2, 8, 8 },
        { 13, 9, 2 },
        { 19, 35, 1 },
        { 24, 49, 5 },
        { 33, 56, 6 },
        { 43, 34, 6 },
        { 53, 52, 12 },
        { 70, 627, 1 },
        { 76, 629, 2 },
        { 83, 628, 2 },
        { 90, 629, 2 },
        { 97, 628, 2, },
        { 103, 10, 3 }
    };
    check(decoder.tokens(), std::span(expectedTokens, std::size(expectedTokens)));
}
}
