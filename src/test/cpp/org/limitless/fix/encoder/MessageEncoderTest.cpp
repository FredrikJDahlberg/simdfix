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
    LogonEncoder logon{};
    logon.wrap(buffer)
            .msgType(MessageType::Logon)
            .sender("SENDER")
            .target("TARGET")
            .sequenceNumber(1)
            .sendingTime(std::chrono::milliseconds{1'781'378'773'959})
            .encryptMethod(Encryption::None)
            .heartbeatInterval(30);

    const auto length = logon.encodedLength();
    const std::string_view encoded{reinterpret_cast<const char*>(buffer.data()), length};

    EXPECT_EQ("35=A" SOH "49=SENDER" SOH "56=TARGET" SOH "34=1" SOH "52=20260613-19:26:13.959" SOH
              "98=0" SOH "108=30" SOH, encoded);
}

}