//
// Created by Fredrik Dahlberg on 2026-06-13.
//

#include <gtest/gtest.h>

#include "org/limitless/fix/utils/Utils.hpp"
#include "org/limitless/fix/messages/FixMessageEncoders.hpp"

namespace org::limitless::fix::encoder {

using namespace fix::messages;

TEST(MessageEncoder, Logon)
{
    uint8_t buffer[256];
    LogonEncoder logon{};
    logon.wrap(buffer)
            .sequenceNumber(1).encryptMethod(Encryption::None)
            .sendingTime(std::chrono::milliseconds{1'781'378'773'959})
            .hops(2)
            .next().hopCompID("hepp").hopRefID(2)
            .next().hopCompID("hopp");
    utils::print(std::size(buffer), buffer);
}

}