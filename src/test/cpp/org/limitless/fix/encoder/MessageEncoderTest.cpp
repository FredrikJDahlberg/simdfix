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
    logon.wrap(buffer, encoder.bodyOffset())
            .sequenceNumber(1)
            .sendingTime(std::chrono::milliseconds{1'781'378'773'959})
            .encryptMethod(Encryption::None)
            .heartbeatInterval(30);

    const auto length = encoder.encode(logon);
    const std::string_view encoded{reinterpret_cast<const char*>(buffer.data()), length};

    EXPECT_EQ("8=FIXT.1.1" SOH "9=0067" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH
              "34=1" SOH "52=20260613-19:26:13.959" SOH "98=0" SOH "108=30" SOH "10=074" SOH, encoded);
}

TEST(MessageEncoder, HeartbeatWithHops)
{
    std::array<uint8_t, 256> buffer{};
    FixPayloadEncoder<"FIXT.1.1", "TARGET", "SENDER"> encoder{};
    encoder.wrap(0, buffer);

    HeartbeatEncoder heartbeat{};
    heartbeat.wrap(buffer, encoder.bodyOffset())
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

}