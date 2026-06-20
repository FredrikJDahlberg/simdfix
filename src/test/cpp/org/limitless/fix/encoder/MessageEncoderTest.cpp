//
// Created by Fredrik Dahlberg on 2026-06-13.
//

#include <gtest/gtest.h>

#include "org/limitless/fix/messages/FixMessageEncoders.hpp"

namespace org::limitless::fix::encoder {

using namespace fix::messages;

#define SOH "\x01"

TEST(MessageEncoder, Logon)
{
    std::array<uint8_t, 256> buffer{};
    FixPayloadEncoder<FIXT_1_1, "TARGET", "SENDER"> encoder{};
    encoder.wrap(0, buffer);

    LogonEncoder logon{};
    encoder.wrapMessage(logon)
            .sequenceNumber(1)
            .sendingTime(std::chrono::milliseconds{1'781'378'773'959})
            .encryptMethod(Encryption::None)
            .heartbeatInterval(30);

    const auto length = encoder.encode(logon);
    const std::string_view encoded{reinterpret_cast<const char*>(buffer.data()), length};

    EXPECT_EQ("8=FIXT.1.1" SOH "9=0067" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH
              "34=1" SOH "52=20260613-19:26:13.959" SOH "98=0" SOH "108=30" SOH "10=074" SOH, encoded);
}

TEST(MessageEncoder, LogonNullEncryption)
{
    std::array<uint8_t, 256> buffer{};
    FixPayloadEncoder<FIXT_1_1, "TARGET", "SENDER"> encoder{};
    encoder.wrap(0, buffer);

    LogonEncoder logon{};
    encoder.wrapMessage(logon)
            .sequenceNumber(1)
            .sendingTime(std::chrono::milliseconds{1'781'378'773'959})
            .encryptMethod(Encryption::Null)
            .heartbeatInterval(30);

    const auto length = encoder.encode(logon);
    const std::string_view encoded{reinterpret_cast<const char*>(buffer.data()), length};

    EXPECT_EQ("8=FIXT.1.1" SOH "9=0062" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH
              "34=1" SOH "52=20260613-19:26:13.959" SOH "108=30" SOH "10=102" SOH, encoded);
}

TEST(MessageEncoder, HeartbeatWithHops)
{
    std::array<uint8_t, 256> buffer{};
    FixPayloadEncoder<FIXT_1_1, "TARGET", "SENDER"> encoder{};
    encoder.wrap(0, buffer);

    HeartbeatEncoder heartbeat{};
    encoder.wrapMessage(heartbeat)
            .sequenceNumber(1)
            .sendingTime(std::chrono::milliseconds{1'781'378'773'959});

    heartbeat
            .hops(3)
            .next().hopCompID("HOP1").hopSendingTime(std::chrono::milliseconds{1'781'378'773'959}).hopRefID(1)
            .next().hopCompID("HOP2").hopSendingTime(std::chrono::milliseconds{1'781'378'773'959}).hopRefID(2)
            .next().hopCompID("HOP3").hopSendingTime(std::chrono::milliseconds{1'781'378'773'959}).hopRefID(3);

    const auto length = encoder.encode(heartbeat);
    const std::string_view encoded{reinterpret_cast<const char*>(buffer.data()), length};

    EXPECT_EQ("8=FIXT.1.1" SOH "9=0184" SOH "35=0" SOH "49=SENDER" SOH "56=TARGET" SOH
              "34=1" SOH "52=20260613-19:26:13.959" SOH
              "627=3" SOH
              "628=HOP1" SOH "629=20260613-19:26:13.959" SOH "630=1" SOH
              "628=HOP2" SOH "629=20260613-19:26:13.959" SOH "630=2" SOH
              "628=HOP3" SOH "629=20260613-19:26:13.959" SOH "630=3" SOH
              "10=141" SOH, encoded);
}

TEST(MessageEncoder, NewOrderSingle)
{
    std::array<uint8_t, 256> buffer{};
    FixPayloadEncoder<FIXT_1_1, "TARGET", "SENDER"> encoder{};
    encoder.wrap(0, buffer);

    NewOrderSingleEncoder order{};
    encoder.wrapMessage(order)
            .sequenceNumber(1)
            .sendingTime(std::chrono::milliseconds{1'781'378'773'959})
            .clOrdID("ORDER1")
            .handlInst(HandlInst::AutoPrivate)
            .symbol("AAPL")
            .side(Side::Buy)
            .transactTime(std::chrono::milliseconds{1'781'378'773'959})
            .orderQty(100)
            .ordType(OrdType::Limit)
            .price(utils::FixedDecimal{120, 0});

    const auto length = encoder.encode(order);
    const std::string_view encoded{reinterpret_cast<const char*>(buffer.data()), length};

    EXPECT_EQ("8=FIXT.1.1" SOH "9=0136" SOH "35=D" SOH "49=SENDER" SOH "56=TARGET" SOH
              "34=1" SOH "52=20260613-19:26:13.959" SOH
              "11=ORDER1" SOH "21=1" SOH "55=AAPL" SOH "54=1" SOH "60=20260613-19:26:13.959" SOH
              "38=100" SOH "40=2" SOH "44=120.00000000" SOH "10=199" SOH, encoded);
}

TEST(MessageEncoder, ResendRequest)
{
    std::array<uint8_t, 256> buffer{};
    FixPayloadEncoder<FIXT_1_1, "TARGET", "SENDER"> encoder{};
    encoder.wrap(0, buffer);

    ResendRequestEncoder resend{};
    encoder.wrapMessage(resend)
            .sequenceNumber(5)
            .sendingTime(std::chrono::milliseconds{1'781'378'773'959})
            .beginSeqNo(1)
            .endSeqNo(4);

    const auto length = encoder.encode(resend);
    const std::string_view encoded{reinterpret_cast<const char*>(buffer.data()), length};

    EXPECT_EQ("8=FIXT.1.1" SOH "9=0064" SOH "35=2" SOH "49=SENDER" SOH "56=TARGET" SOH
              "34=5" SOH "52=20260613-19:26:13.959" SOH "7=1" SOH "16=4" SOH "10=162" SOH, encoded);
}

TEST(MessageEncoder, Reject)
{
    std::array<uint8_t, 256> buffer{};
    FixPayloadEncoder<FIXT_1_1, "TARGET", "SENDER"> encoder{};
    encoder.wrap(0, buffer);

    RejectEncoder reject{};
    encoder.wrapMessage(reject)
            .sequenceNumber(10)
            .sendingTime(std::chrono::milliseconds{1'781'378'773'959})
            .refSeqNum(9)
            .refTagID(55)
            .refMsgType(MessageType::NewOrderSingle)
            .sessionRejectReason(SessionRejectReason::RequiredTagMissing)
            .text("Missing Symbol");

    const auto length = encoder.encode(reject);
    const std::string_view encoded{reinterpret_cast<const char*>(buffer.data()), length};

    EXPECT_EQ("8=FIXT.1.1" SOH "9=0098" SOH "35=3" SOH "49=SENDER" SOH "56=TARGET" SOH
              "34=10" SOH "52=20260613-19:26:13.959" SOH "45=9" SOH "371=55" SOH "372=D" SOH
              "373=1" SOH "58=Missing Symbol" SOH "10=191" SOH, encoded);
}

TEST(MessageEncoder, RejectMinimal)
{
    std::array<uint8_t, 256> buffer{};
    FixPayloadEncoder<FIXT_1_1, "TARGET", "SENDER"> encoder{};
    encoder.wrap(0, buffer);

    RejectEncoder reject{};
    encoder.wrapMessage(reject)
            .sequenceNumber(3)
            .sendingTime(std::chrono::milliseconds{1'781'378'773'959})
            .refSeqNum(2);

    const auto length = encoder.encode(reject);
    const std::string_view encoded{reinterpret_cast<const char*>(buffer.data()), length};

    EXPECT_EQ("8=FIXT.1.1" SOH "9=0060" SOH "35=3" SOH "49=SENDER" SOH "56=TARGET" SOH
              "34=3" SOH "52=20260613-19:26:13.959" SOH "45=2" SOH "10=247" SOH, encoded);
}

TEST(MessageEncoder, SequenceReset)
{
    std::array<uint8_t, 256> buffer{};
    FixPayloadEncoder<FIXT_1_1, "TARGET", "SENDER"> encoder{};
    encoder.wrap(0, buffer);

    SequenceResetEncoder seqReset{};
    encoder.wrapMessage(seqReset)
            .sequenceNumber(5)
            .sendingTime(std::chrono::milliseconds{1'781'378'773'959})
            .gapFillFlag(GapFillFlag::GapFillMessage)
            .newSeqNo(10);

    const auto length = encoder.encode(seqReset);
    const std::string_view encoded{reinterpret_cast<const char*>(buffer.data()), length};

    EXPECT_EQ("8=FIXT.1.1" SOH "9=0067" SOH "35=4" SOH "49=SENDER" SOH "56=TARGET" SOH
              "34=5" SOH "52=20260613-19:26:13.959" SOH "123=Y" SOH "36=10" SOH "10=093" SOH, encoded);
}

TEST(MessageEncoder, SequenceResetNoGapFill)
{
    std::array<uint8_t, 256> buffer{};
    FixPayloadEncoder<FIXT_1_1, "TARGET", "SENDER"> encoder{};
    encoder.wrap(0, buffer);

    SequenceResetEncoder seqReset{};
    encoder.wrapMessage(seqReset)
            .sequenceNumber(5)
            .sendingTime(std::chrono::milliseconds{1'781'378'773'959})
            .newSeqNo(10);

    const auto length = encoder.encode(seqReset);
    const std::string_view encoded{reinterpret_cast<const char*>(buffer.data()), length};

    EXPECT_EQ("8=FIXT.1.1" SOH "9=0061" SOH "35=4" SOH "49=SENDER" SOH "56=TARGET" SOH
              "34=5" SOH "52=20260613-19:26:13.959" SOH "36=10" SOH "10=042" SOH, encoded);
}

TEST(MessageEncoder, LogonWithXmlData)
{
    std::array<uint8_t, 512> buffer{};
    FixPayloadEncoder<FIXT_1_1, "TARGET", "SENDER"> encoder{};
    encoder.wrap(0, buffer);

    const std::array<uint8_t, 11> xml = {'<', 'r', 'o', 'o', 't', '/', '>', 't', 'e', 's', 't'};

    LogonEncoder logon{};
    encoder.wrapMessage(logon)
            .sequenceNumber(1)
            .sendingTime(std::chrono::milliseconds{1'781'378'773'959})
            .encryptMethod(Encryption::None)
            .heartbeatInterval(30)
            .xmlData(xml);

    const auto length = encoder.encode(logon);
    const std::string_view encoded{reinterpret_cast<const char*>(buffer.data()), length};

    EXPECT_EQ("8=FIXT.1.1" SOH "9=0090" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH
              "34=1" SOH "52=20260613-19:26:13.959" SOH "98=0" SOH "108=30" SOH
              "212=11" SOH "213=<root/>test" SOH "10=124" SOH, encoded);
}

}