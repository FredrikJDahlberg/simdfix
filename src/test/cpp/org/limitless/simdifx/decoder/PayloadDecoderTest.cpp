//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "org/limitless/simdifx/utils/Conversions.hpp"
#include "org/limitless/simdifx/decoder/PayloadDecoder.hpp"

#include "org/limitless/simdifx/generated/messages/FixMessageDecoders.hpp"
#include "org/limitless/simdifx/generated/messages/FixTypes.hpp"

namespace org::limitless::simdifx::decoder {

using namespace org::limitless::simdifx::generated::config;
using namespace org::limitless::simdifx::generated::messages;

#define SOH "\x01"

// Copies into an exact-size heap buffer so AddressSanitizer flags any read past
// the logical end (a string-literal span leaves a readable '\0' after the data).
[[nodiscard]] inline std::vector<uint8_t> heap(const std::span<const uint8_t> source)
{
    return {source.begin(), source.end()};
}

// Assembles a well-formed message from its body fields, filling in the
// BodyLength and CheckSum the bytes call for.
[[nodiscard]] inline std::string build(const std::vector<std::string>& body)
{
    std::string fields;
    for (const auto& field : body)
    {
        fields += field;
        fields += static_cast<char>(FieldEnd);
    }
    std::string message = "8=FIXT.1.1" SOH "9=" + std::to_string(fields.size()) + SOH + fields;
    uint32_t sum = 0;
    for (const auto byte : message)
    {
        sum += static_cast<uint8_t>(byte);
    }
    const auto checkSum = std::to_string(sum & 0xff);
    return message + "10=" + std::string(3 - checkSum.size(), '0') + checkSum + SOH;
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
    PayloadDecoder<Protocol::FIXT_1_1> decoder;
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
    PayloadDecoder<Protocol::FIXT_1_1> decoder;
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

TEST(PayloadDecoder, ReusedDecoderSecondMessageFieldLengths)
{
    PayloadDecoder<Protocol::FIXT_1_1> decoder;
    const auto logon = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=66" SOH "35=A" SOH "49=CLIENT" SOH "56=SEQUENCER" SOH
        "34=1" SOH "52=20260703-12:00:00" SOH "98=0" SOH "108=30" SOH "10=227" SOH);
    auto [processed1, status1] = decoder.parse(logon);
    ASSERT_EQ(Result::Success, status1);
    ASSERT_EQ(logon.size(), processed1);

    const auto heartbeat = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=59" SOH "35=0" SOH "49=WRONGSENDER" SOH "56=SEQUENCER" SOH
        "34=2" SOH "52=20260703-12:00:00" SOH "10=075" SOH);
    auto [processed2, status2] = decoder.parse(heartbeat);
    ASSERT_EQ(Result::Success, status2);
    ASSERT_EQ(heartbeat.size(), processed2);

    constexpr Field expectedFields[] =
    {
        { 2, 8, 8 },
        { 13, 9, 2 },
        { 19, 35, 1 },
        { 24, 49, 11 },
        { 39, 56, 9 },
        { 52, 34, 1 },
        { 57, 52, 17 },
        { 78, 10, 3 },
    };
    check(decoder.fields(), std::span(expectedFields, std::size(expectedFields)));
}

TEST(PayloadDecoder, TrailerFieldEnd)
{
    const auto message = utils::makeSpan("8=FIXT.1.1" SOH "9=21" SOH "35=66" SOH
        "666=66" SOH "1=1" SOH "2=2" SOH "10=233" SOH);
    PayloadDecoder<Protocol::FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::Success, status);
}

TEST(PayloadDecoder, BlockBoundaryFieldEnd)
{
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=69" SOH "35=2" SOH "49=CLIENT" SOH
        "56=SEQUENCER2" SOH "34=10" SOH "52=20260101-00:00:00.000" SOH
        "7=1" SOH "16=0" SOH "10=078" SOH);
    PayloadDecoder<Protocol::FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::Success, status);
    ASSERT_EQ(message.size(), processed);

    const auto fields = decoder.fields();
    // Tag 7 (BeginSeqNo) must be tokenized with value at byte 78, length 1.
    ASSERT_EQ(7,  fields[7].m_tag);
    ASSERT_EQ(78, fields[7].m_position);
    ASSERT_EQ(1,  fields[7].m_length);
    // Tag 16 (EndSeqNo) must follow immediately in the trailer region.
    ASSERT_EQ(16, fields[8].m_tag);
    ASSERT_EQ(83, fields[8].m_position);
    ASSERT_EQ(1,  fields[8].m_length);
}

// Second instance of the BlockBoundaryFieldEnd bug above: the closing SOH again
// lands on byte 15 of the last full block, but here the field's value *starts* in
// the preceding block (tag 122 at 78, SOH at 95). processBlock's boundary check
// only closed the field when it also began in the same block, so the length stayed
// at 0 and the typed accessor read the field as absent. Shifting the whole message
// by one byte moves the SOH off lane 15 and the same field parses correctly, which
// is what makes this alignment-dependent rather than a plain framing error.
TEST(PayloadDecoder, BlockBoundaryFieldEndSpanningPreviousBlock)
{
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=80" SOH "35=0" SOH "49=CLIENT" SOH "56=SEQUENCERNODE" SOH
        "34=2" SOH "52=20260703-12:00:00" SOH "122=20260703-11:59:00" SOH "10=020" SOH);
    PayloadDecoder<Protocol::FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::Success, status) << name(status);
    ASSERT_EQ(message.size(), processed);

    constexpr Field expectedFields[] =
    {
        { 2, 8, 8 },
        { 13, 9, 2 },
        { 19, 35, 1 },
        { 24, 49, 6 },
        { 34, 56, 13 },
        { 51, 34, 1 },
        { 56, 52, 17 },
        { 78, 122, 17 },
        { 99, 10, 3 },
    };
    check(decoder.fields(), std::span(expectedFields, std::size(expectedFields)));

    // Control: one extra byte ahead of tag 122 moves its SOH to 96 (lane 0).
    const auto shifted = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=81" SOH "35=0" SOH "49=CLIENTX" SOH "56=SEQUENCERNODE" SOH
        "34=2" SOH "52=20260703-12:00:00" SOH "122=20260703-11:59:00" SOH "10=109" SOH);
    PayloadDecoder<Protocol::FIXT_1_1> shiftedDecoder;
    auto [shiftedProcessed, shiftedStatus] = shiftedDecoder.parse(shifted);
    ASSERT_EQ(Result::Success, shiftedStatus) << name(shiftedStatus);
    ASSERT_EQ(shifted.size(), shiftedProcessed);

    const auto fields = shiftedDecoder.fields();
    ASSERT_EQ(122, fields[7].m_tag);
    ASSERT_EQ(79, fields[7].m_position);
    ASSERT_EQ(17, fields[7].m_length);
}

// Both block-boundary bugs above were alignment-dependent: the same field parsed
// correctly at one offset and lost its length at another, and neither showed up as
// a parse failure. The fuzz suite only proves malformed input is never Success, so
// nothing checked that a *valid* message tokenizes to the right positions and
// lengths at every 16-byte alignment. This sweeps a message across all of them by
// growing a padding field, checking each field against the layout of its own bytes.
TEST(PayloadDecoder, EveryAlignmentTokenizesToTheWireLayout)
{
    for (size_t pad = 0; pad <= 32; ++pad)
    {
        const std::string message = build({"35=0", "49=" + std::string(6 + pad, 'C'),
            "56=SEQUENCERNODE", "34=2", "52=20260703-12:00:00", "122=20260703-11:59:00"});
        const std::vector<uint8_t> buffer(message.begin(), message.end());
        PayloadDecoder<Protocol::FIXT_1_1> decoder;
        const auto [processed, status] = decoder.parse(Buffer{buffer.data(), buffer.size()});
        ASSERT_EQ(Result::Success, status) << "pad " << pad << ": " << name(status);
        ASSERT_EQ(buffer.size(), processed) << "pad " << pad;

        const auto fields = decoder.fields();
        for (size_t start = 0; start < message.size(); )
        {
            const size_t equals = message.find('=', start);
            const size_t end = message.find(FieldEnd, start);
            const auto tag = static_cast<uint16_t>(std::stoul(message.substr(start, equals - start)));
            const auto found = std::ranges::find(fields, tag, &Field::m_tag);
            ASSERT_NE(fields.end(), found) << "pad " << pad << ", tag " << tag << " not tokenized";
            EXPECT_EQ(equals + 1, found->m_position) << "pad " << pad << ", tag " << tag;
            EXPECT_EQ(end - equals - 1, found->m_length) << "pad " << pad << ", tag " << tag;
            start = end + 1;
        }
    }
}

TEST(PayloadDecoder, Fragment)
{
    const auto message = utils::makeSpan("8=FIXT.");
    PayloadDecoder<Protocol::FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::MessageFragment, status);
    ASSERT_EQ(0UL, processed);
}

TEST(PayloadDecoder, TrailerSplitValue)
{
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0129" SOH "35=D" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=1" SOH "52=20260613-19:26:13.959" SOH
        "11=ORDER1" SOH "21=1" SOH "55=AAPL" SOH "54=1" SOH "60=20260613-19:26:13.959" SOH
        "38=100" SOH "40=2" SOH "44=15000" SOH "10=126" SOH);
    PayloadDecoder<Protocol::FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::Success, status);
    ASSERT_EQ(message.size(), processed);

    const auto fields = decoder.fields();
    ASSERT_EQ(44, fields[14].m_tag);
    ASSERT_EQ(141, fields[14].m_position);
    ASSERT_EQ(5, fields[14].m_length);
}

TEST(PayloadDecoder, SplitTagDigitZero)
{
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0142" SOH "35=8" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=1" SOH "52=20260613-19:26:13.959" SOH
        "37=ORD002" SOH "17=EXEC002" SOH "150=0" SOH "39=0" SOH
        "55=MSFT" SOH "54=2" SOH "151=200" SOH "14=0" SOH
        "6=0" SOH "60=20260613-19:26:13.959" SOH "10=249" SOH);
    PayloadDecoder<Protocol::FIXT_1_1> decoder;
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
    PayloadDecoder<Protocol::FIXT_1_1> decoder;
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
            PayloadDecoder<Protocol::FIXT_1_1, DataFields> decoder;
            auto [processed, status] = decoder.parse(Buffer{buffer.data(), buffer.size()});
            ASSERT_NE(Result::Success, status) << "truncated to " << n << " bytes";
        }
    }

    // A data length field whose declared payload runs past the buffer end.
    {
        const auto truncated = heap(utils::makeSpan(
            "8=FIXT.1.1" SOH "9=0050" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH
            "34=1" SOH "212=99" SOH "213=<r"));
        PayloadDecoder<Protocol::FIXT_1_1, DataFields> decoder;
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
        PayloadDecoder<Protocol::FIXT_1_1, DataFields> decoder;
        auto [processed, status] = decoder.parse(Buffer{buffer.data(), buffer.size()});
        ASSERT_NE(Result::Success, status) << "garbage length " << n;
    }
}

// A structurally-complete, correctly-checksummed message whose declared
// BodyLength(9) does not match its real length: the CheckSum tag is still
// found (hasCheckSum), so this is reported as InvalidBodyLength — a distinct
// failure from a still-arriving message (Fragment, below) or a truncated one
// (TruncationSafety, above). Found via FixConnectionTest.cpp's
// CorruptBodyLengthIsDiscardedNotDisconnected: phixeron maps this status to
// "discard silently, keep the connection" rather than a Reject or a disconnect.
TEST(PayloadDecoder, CompleteMessageWithWrongBodyLengthValue)
{
    // Real body is 53 bytes; declared BodyLength(9) claims 999.
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=999" SOH "35=0" SOH "49=CLIENT" SOH "56=PHIXERON" SOH
        "34=2" SOH "52=20260703-12:00:00" SOH "10=187" SOH);
    PayloadDecoder<Protocol::FIXT_1_1> decoder;
    const auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::InvalidBodyLength, status);
    ASSERT_EQ(message.size(), processed) << "the whole malformed message is consumed, not held as a fragment";
}

// Same as above, but the BodyLength(9) value itself is non-numeric garbage
// rather than a wrong-but-parseable number. The tokenizer does not validate
// a field's value content while framing (only '=' and SOH positions matter),
// so this still tokenizes as a complete message and is likewise reported as
// InvalidBodyLength once the parsed "value" fails the byte-count cross-check.
TEST(PayloadDecoder, CompleteMessageWithGarbledBodyLengthDigits)
{
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=!@#" SOH "35=0" SOH "49=CLIENT" SOH "56=PHIXERON" SOH
        "34=2" SOH "52=20260703-12:00:00" SOH "10=148" SOH);
    PayloadDecoder<Protocol::FIXT_1_1> decoder;
    const auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::InvalidBodyLength, status);
    ASSERT_EQ(message.size(), processed);
}

// A BodyLength(9) claiming far more bytes than are actually on the wire, with
// no CheckSum anywhere in what's present: hasCheckSum is false, so this reads
// as "still arriving" (MessageFragment, processed=0), not as malformed — the
// tokenizer cannot yet tell a huge-but-honest message from a corrupt one.
// Found via FixConnectionTest.cpp's OversizedBodyLengthNeverArrivingEventuallyDisconnects:
// phixeron disconnects such a connection only once the held fragment exceeds
// its own MaxMessageSize cap, independent of this status.
TEST(PayloadDecoder, OversizedBodyLengthWithoutCheckSumIsFragment)
{
    std::string message = "8=FIXT.1.1" SOH "9=5000000" SOH "35=0" SOH "49=CLIENT" SOH "56=PHIXERON" SOH
        "34=2" SOH "52=20260703-12:00:00" SOH;
    message += std::string(100, 'Z');  // filler: no '=', no SOH, no CheckSum ever appears
    const std::vector<uint8_t> buffer(message.begin(), message.end());
    PayloadDecoder<Protocol::FIXT_1_1> decoder;
    const auto [processed, status] = decoder.parse(Buffer{buffer.data(), buffer.size()});
    ASSERT_EQ(Result::MessageFragment, status);
    ASSERT_EQ(0UL, processed);
}

// Regression for a bug found while adding this coverage: unlike skipDataField's
// `padded` (gated on field length <= 8), checkRequiredFields' bodyPadded used to
// ignore bodyLength's own digit count, so any BodyLength value written with more
// than 8 digits hit asciiToUint64's SWAR fast path, which silently reads only
// the first 8 digits and drops the rest (the same root cause fixed for
// FieldDecoder.hpp's convertToUint32/convertToInt32 earlier). A correct value
// merely padded with leading zeros past 8 digits must still parse correctly.
TEST(PayloadDecoder, NineDigitBodyLengthWithLeadingZerosParsesCorrectly)
{
    // Real body is 53 bytes; declared BodyLength(9) is the same value, zero-padded to 9 digits.
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=000000053" SOH "35=0" SOH "49=CLIENT" SOH "56=PHIXERON" SOH
        "34=2" SOH "52=20260703-12:00:00" SOH "10=200" SOH);
    PayloadDecoder<Protocol::FIXT_1_1> decoder;
    const auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::Success, status) << name(status);
    ASSERT_EQ(message.size(), processed);
}

// Symmetric case for the length-prefixed DATA field mechanism (skipDataField):
// unlike BodyLength above, its `padded` flag already gates on field length <= 8
// (PayloadDecoder.hpp's skipDataField), so a DataLen value written with more
// than 8 digits was never subject to the SWAR truncation bug. This pins that
// down so a future change to skipDataField can't silently regress it.
TEST(PayloadDecoder, NineDigitDataLengthWithLeadingZerosParsesCorrectly)
{
    struct DataFields
    {
        static constexpr int32_t dataTag(const uint16_t tag) { return tag == 212 ? 213 : -1; }
    };
    // Real DataLen(212) is 12 bytes (same payload as the ValidData fixture elsewhere
    // in this file), declared zero-padded to 9 digits.
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=98" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH "34=1" SOH
        "52=20260613-19:26:13.959" SOH "98=0" SOH "108=30" SOH "212=000000012" SOH "213=<root" SOH
        "/>test" SOH "10=118" SOH);
    PayloadDecoder<Protocol::FIXT_1_1, DataFields> decoder;
    const auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::Success, status) << name(status);
    ASSERT_EQ(message.size(), processed);

    const auto fields = decoder.fields();
    bool found = false;
    for (const auto& field : fields)
    {
        if (field.m_tag == 213)
        {
            EXPECT_EQ(12U, field.m_length) << "the 9-digit DataLen must parse as 12, not a truncated value";
            found = true;
            break;
        }
    }
    ASSERT_TRUE(found);
}

// A DataLen(212) value that is non-numeric garbage rather than a wrong-but-
// parseable number: the tokenizer does not validate a field's value content
// while framing, so this still frames as a complete-looking message and the
// garbage-derived length must not be mistaken for a valid one.
TEST(PayloadDecoder, DataFieldWithGarbledLengthDigitsIsNotSuccess)
{
    struct DataFields
    {
        static constexpr int32_t dataTag(const uint16_t tag) { return tag == 212 ? 213 : -1; }
    };
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=92" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH "34=1" SOH
        "52=20260613-19:26:13.959" SOH "98=0" SOH "108=30" SOH "212=!@#" SOH "213=<root" SOH
        "/>test" SOH "10=065" SOH);
    PayloadDecoder<Protocol::FIXT_1_1, DataFields> decoder;
    const auto [processed, status] = decoder.parse(message);
    ASSERT_NE(Result::Success, status) << name(status);
    ASSERT_LE(processed, message.size());
}

TEST(PayloadDecoder, ForeignBeginStringSkip)
{
        for (const std::string_view text : {
        "8=FIX.4.4" SOH "9=117" SOH "35=A" SOH "49=Buyer" SOH "56=SellerSide" SOH "34=1" SOH
        "52=20190605-11:51:27.84800" SOH "1128=9" SOH "98=0" SOH "108=30" SOH "141=Y" SOH
        "553=Username" SOH "554=Password" SOH "1137=9" SOH "10=218" SOH,
        "8=FIX.4.4" SOH "9=0091" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH,
        "................................"})
    {
        const std::vector<uint8_t> buffer(text.begin(), text.end());
        PayloadDecoder<Protocol::FIXT_1_1> decoder;
        const auto [processed, status] = decoder.parse(Buffer{buffer.data(), buffer.size()});
        ASSERT_EQ(Result::InvalidBeginString, status) << text << " -> " << name(status);
        ASSERT_EQ(0, processed) << text;
    }
    for (const std::string_view text : {"xxxxxxxxxxxxxxxx8=FIXT.1.1" SOH "9=0091" SOH "35=A" SOH,
                                        "x8=FIXT.1.1" SOH "9=0091" SOH "35=A" SOH "49=SENDER" SOH})
    {
        const std::vector<uint8_t> buffer(text.begin(), text.end());
        PayloadDecoder<Protocol::FIXT_1_1> decoder;
        const auto [processed, status] = decoder.parse(Buffer{buffer.data(), buffer.size()});
        ASSERT_EQ(Result::InvalidBeginString, status) << text << " -> " << name(status);
        ASSERT_EQ(0, processed) << "the junk, and not one byte of what follows it";
    }
}

}
