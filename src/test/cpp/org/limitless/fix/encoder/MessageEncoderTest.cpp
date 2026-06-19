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
    FixPayloadEncoder<"FIXT.1.1", "TARGET", "SENDER"> encoder{};
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
    FixPayloadEncoder<"FIXT.1.1", "TARGET", "SENDER"> encoder{};
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
    FixPayloadEncoder<"FIXT.1.1", "TARGET", "SENDER"> encoder{};
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
    FixPayloadEncoder<"FIXT.1.1", "TARGET", "SENDER"> encoder{};
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
            .price(15000);

    const auto length = encoder.encode(order);
    const std::string_view encoded{reinterpret_cast<const char*>(buffer.data()), length};

    EXPECT_EQ("8=FIXT.1.1" SOH "9=0129" SOH "35=D" SOH "49=SENDER" SOH "56=TARGET" SOH
              "34=1" SOH "52=20260613-19:26:13.959" SOH
              "11=ORDER1" SOH "21=1" SOH "55=AAPL" SOH "54=1" SOH "60=20260613-19:26:13.959" SOH
              "38=100" SOH "40=2" SOH "44=15000" SOH "10=126" SOH, encoded);
}

}