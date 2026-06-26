//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#include <vector>

#include <gtest/gtest.h>

#include "org/limitless/fix/utils/Conversions.hpp"
#include "org/limitless/fix/decoder/PayloadDecoder.hpp"
#include "org/limitless/fix/messages/FixTypes.hpp"

namespace org::limitless::fix::decoder {

#define SOH "\x01"

// Copies into an exact-size heap buffer so AddressSanitizer flags any read past
// the logical end (a string-literal span leaves a readable '\0' after the data).
[[nodiscard]] inline std::vector<uint8_t> heap(const std::span<const uint8_t> source)
{
    return {source.begin(), source.end()};
}

void check(std::span<Field> result, const std::span<const Field> expected)
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

TEST(PayloadDecoder, Basics)
{
    const auto message = utils::makeSpan("8=FIXT.1.1" SOH "9=118" SOH "35=A" SOH "49=Buyer" SOH
        "56=SellerSide" SOH "34=1" SOH "52=20190605-11:51:27.84800" SOH "1128=9" SOH "98=0" SOH "108=30" SOH
        "141=Y" SOH "553=Username" SOH "554=Password" SOH "1137=9" SOH "10=218" SOH
        // next message
        "8=FIXT.1.1" SOH "9=118" SOH);
    PayloadDecoder<config::FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::Success, status);
    ASSERT_EQ(message.size() - 17, processed);
    // ASSERT_EQ(218, checkSum);
    constexpr Field expectedFields[] =
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
    check(decoder.fields(), std::span(expectedFields, std::size(expectedFields)));
}

TEST(PayloadDecoder, TrailerSplitCheckSum)
{
    const auto message = utils::makeSpan("8=FIXT.1.1" SOH "9=47" SOH "35=A" SOH
        "49=Buyer" SOH "56=Seller" SOH "34=2000001" SOH "52=20190605" SOH "10=046" SOH);
    PayloadDecoder<config::FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::Success, status);
    ASSERT_EQ(message.size(), processed);

    constexpr Field expectedFields[] =
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
    check(decoder.fields(), std::span(expectedFields, std::size(expectedFields)));
}

TEST(PayloadDecoder, TrailerFieldEnd)
{
    const auto message = utils::makeSpan("8=FIXT.1.1" SOH "9=21" SOH "35=66" SOH
        "666=66" SOH "1=1" SOH "2=2" SOH "10=233" SOH);
    PayloadDecoder<config::FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::Success, status);
}

TEST(PayloadDecoder, Fragment)
{
    const auto message = utils::makeSpan("8=FIXT.");
    PayloadDecoder<config::FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::MessageFragment, status);
    ASSERT_EQ(0UL, processed);
}

TEST(PayloadDecoder, TrailerSplitValue)
{
    // Regression test for processTrailer's split-value branch.
    //
    // The field value "15000" (tag 44) has its first three bytes ('1','5','0')
    // at the end of the last 16-byte NEON block and the remaining two ('0','0')
    // plus the SOH in the tail.  The tail's first SOH therefore precedes the
    // tail's first '=', triggering the split-value branch.
    // Before the fix the branch computed fieldEndPos-tagEndPos-1 = 2 instead of
    // (offset-token.m_position)+fieldEndPos = 5, silently truncating the value.
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0129" SOH "35=D" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=1" SOH "52=20260613-19:26:13.959" SOH
        "11=ORDER1" SOH "21=1" SOH "55=AAPL" SOH "54=1" SOH "60=20260613-19:26:13.959" SOH
        "38=100" SOH "40=2" SOH "44=15000" SOH "10=126" SOH);
    PayloadDecoder<config::FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::Success, status);
    ASSERT_EQ(message.size(), processed);

    // Field 14 is tag 44 (Price); its value "15000" crosses the block→tail
    // boundary.  Verify the full five-digit length is reported, not the
    // truncated two-digit length produced by the pre-fix formula.
    const auto fields = decoder.fields();
    ASSERT_EQ(44, fields[14].m_tag);
    ASSERT_EQ(141, fields[14].m_position);
    ASSERT_EQ(5, fields[14].m_length);
}

TEST(PayloadDecoder, SplitTagDigitZero)
{
    // Tag 150 ("150=0") starts at byte 94, placing the '0' digit of "150"
    // at position 96 — the first byte of a new 16-byte SIMD chunk. The
    // previous chunk carries "15" via m_tag. The digit '0' maps to value 0
    // after subtracting '0', which is identical to the "no digit" sentinel.
    // Before the fix, processBlock treated it as a non-digit and emitted
    // tag 15 instead of 150.
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0142" SOH "35=8" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=1" SOH "52=20260613-19:26:13.959" SOH
        "37=ORD002" SOH "17=EXEC002" SOH "150=0" SOH "39=0" SOH
        "55=MSFT" SOH "54=2" SOH "151=200" SOH "14=0" SOH
        "6=0" SOH "60=20260613-19:26:13.959" SOH "10=249" SOH);
    PayloadDecoder<config::FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::Success, status);
    ASSERT_EQ(message.size(), processed);

    const auto fields = decoder.fields();
    ASSERT_EQ(150, fields[9].m_tag);
    ASSERT_EQ(98, fields[9].m_position);
    ASSERT_EQ(1, fields[9].m_length);
}

TEST(PayloadDecoder, HopGroup)
{
    const auto logout = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=84" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=10:11:12.123" SOH
        "627=2" SOH "629=10" SOH "628=12" SOH "629=37" SOH "628=20" SOH "10=211" SOH);
    PayloadDecoder<config::FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(logout);
    ASSERT_EQ(Result::Success, status);
    constexpr Field expectedFields[] =
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
    check(decoder.fields(), std::span(expectedFields, std::size(expectedFields)));
}

// Buffer-safety regression: truncated and malformed messages must never read
// out of bounds. Every buffer below is an exact-size heap allocation, so any
// over-read trips an AddressSanitizer redzone (the Debug build links ASan).
TEST(PayloadDecoder, TruncationSafety)
{
    struct DataFields
    {
        static constexpr int32_t dataTag(const uint16_t tag) { return tag == 212 ? 213 : -1; }
    };

    const auto logon = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=118" SOH "35=A" SOH "49=Buyer" SOH "56=SellerSide" SOH "34=1" SOH
        "52=20190605-11:51:27.84800" SOH "1128=9" SOH "98=0" SOH "108=30" SOH "141=Y" SOH
        "553=Username" SOH "554=Password" SOH "1137=9" SOH "10=218" SOH);
    const auto data = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0091" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH "34=1" SOH
        "52=20260613-19:26:13.959" SOH "98=0" SOH "108=30" SOH "212=12" SOH "213=<root" SOH
        "/>test" SOH "10=127" SOH);

    // Every truncation of each message: must not crash, and an incomplete
    // message must never report Success.
    for (const auto full : {Buffer{logon}, Buffer{data}})
    {
        for (size_t n = 0; n < full.size(); ++n)
        {
            const auto buffer = heap(full.first(n));
            PayloadDecoder<config::FIXT_1_1, DataFields> decoder;
            auto [processed, status] = decoder.parse(Buffer{buffer.data(), buffer.size()});
            ASSERT_NE(Result::Success, status) << "truncated to " << n << " bytes";
        }
    }

    // A data length field whose declared payload runs past the buffer end.
    {
        const auto truncated = heap(utils::makeSpan(
            "8=FIXT.1.1" SOH "9=0050" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH
            "34=1" SOH "212=99" SOH "213=<r"));
        PayloadDecoder<config::FIXT_1_1, DataFields> decoder;
        auto [processed, status] = decoder.parse(Buffer{truncated.data(), truncated.size()});
        ASSERT_NE(Result::Success, status);
    }

    // Garbage body behind a valid BeginString prefix, various lengths.
    for (size_t n = 32; n <= data.size(); ++n)
    {
        auto buffer = heap(data.first(n));
        for (size_t i = 11; i < buffer.size(); ++i)
        {
            buffer[i] = static_cast<uint8_t>((i * 37 + 13) & 0xff);
        }
        PayloadDecoder<config::FIXT_1_1, DataFields> decoder;
        auto [processed, status] = decoder.parse(Buffer{buffer.data(), buffer.size()});
        ASSERT_NE(Result::Success, status) << "garbage length " << n;
    }
}
}
