//
// Created by Fredrik Dahlberg on 2026-08-01.
//

#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "org/limitless/simdifx/decoder/PayloadDecoder.hpp"
#include "org/limitless/simdifx/encoder/ResendEncoder.hpp"
#include "org/limitless/simdifx/generated/messages/FixMessageEncoders.hpp"

namespace org::limitless::simdifx::encoder
{

using namespace std::chrono;
using namespace simdifx::generated::config;
using namespace simdifx::generated::messages;

#define SOH "\x01"

static std::vector<uint8_t> encodeNewOrderSingle(const uint32_t seqNum, const milliseconds sendingTime)
{
    std::vector<uint8_t> buffer(256);
    FixPayloadEncoder encoder{Protocol::FIXT_1_1, "SENDER", "TARGET"};
    encoder.wrap(0, buffer);

    NewOrderSingleEncoder order{};
    encoder.wrapMessage(order)
            .sequenceNumber(seqNum)
            .sendingTime(sendingTime)
            .clOrdID("ORDER1")
            .handlInst(HandlInst::AutoPrivate)
            .symbol("AAPL")
            .side(Side::Buy)
            .transactTime(sendingTime)
            .orderQty(100)
            .ordType(OrdType::Limit);

    const auto length = encoder.encode(order);
    buffer.resize(length);
    return buffer;
}

TEST(ResendEncoder, StampsCurrentSendingTimeSetsPossDupAndPreservesOriginal)
{
    const auto original = encodeNewOrderSingle(2, milliseconds{1'781'378'773'959});

    std::array<uint8_t, 512> out{};
    const auto length = encodeResend<Protocol::FIXT_1_1>(Buffer{original.data(), original.size()}, out,
                                                          milliseconds{1'781'378'778'959});
    ASSERT_NE(0u, length);
    const std::string_view resent{reinterpret_cast<const char*>(out.data()), length};

    EXPECT_NE(std::string_view::npos, resent.find(SOH "34=2" SOH)) << "MsgSeqNum unchanged";
    EXPECT_NE(std::string_view::npos, resent.find(SOH "52=20260613-19:26:18.959" SOH))
        << "SendingTime is this retransmission's own time, not the original";
    EXPECT_NE(std::string_view::npos, resent.find(SOH "43=Y" SOH)) << "PossDupFlag=Y";
    EXPECT_NE(std::string_view::npos, resent.find(SOH "122=20260613-19:26:13.959" SOH))
        << "OrigSendingTime preserves the original SendingTime";
    EXPECT_NE(std::string_view::npos, resent.find(SOH "55=AAPL" SOH)) << "body fields copied through unprocessed";
    EXPECT_NE(std::string_view::npos, resent.find(SOH "38=100" SOH)) << "body fields copied through unprocessed";

    // The patched message must itself decode as valid, well-formed FIX (correct BodyLength/CheckSum) —
    // not just contain the right substrings.
    decoder::PayloadDecoder<Protocol::FIXT_1_1> decoder;
    const auto result = decoder.parse(Buffer{out.data(), length});
    EXPECT_EQ(Result::Success, result.m_status);
}

TEST(ResendEncoder, ReturnsZeroForGarbledInput)
{
    const std::array<uint8_t, 64> garbage{};
    std::array<uint8_t, 512> out{};
    EXPECT_EQ(0u, encodeResend<Protocol::FIXT_1_1>(Buffer{garbage.data(), garbage.size()}, out, milliseconds{0}));
}

TEST(ResendEncoder, ReturnsZeroWhenOutputBufferIsTooSmall)
{
    const auto original = encodeNewOrderSingle(2, milliseconds{1'781'378'773'959});
    std::array<uint8_t, 8> tooSmall{};
    EXPECT_EQ(0u, encodeResend<Protocol::FIXT_1_1>(Buffer{original.data(), original.size()}, tooSmall,
                                                    milliseconds{1'781'378'778'959}));
}

}  // namespace org::limitless::simdifx::encoder
