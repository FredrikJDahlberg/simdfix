//
// Created by Fredrik Dahlberg on 2026-04-24.
//
#include <gtest/gtest.h>

#include "org/limitless/fix/parser/Parser.hpp"
#include "org/limitless/fix/parser/Dictionary.hpp"
#include "org/limitless/fix/messages/Grammar.hpp"

namespace org::limitless::fix::parser {


void handler(Message* message)
{
    auto logon = reinterpret_cast<generated::Logon*>(message);
    auto sender = logon->sender();
    auto target = logon->target();
    auto sequenceNumber = logon->expectedSeqNum();
#if 0
    auto tok141 = parser.nextByTag(141);
    ASSERT_TRUE(tok141 != nullptr && tok141->tag == 141);
    auto tok1137 = parser.nextByTag(1137);
    ASSERT_TRUE(tok1137 != nullptr && tok1137->tag == 1137);
    auto tok554 = parser.nextByTag(554);
    ASSERT_TRUE(tok554 != nullptr && tok554->tag == 554);
#endif
}

TEST(Parser, Dictionary)
{
    using namespace org::limitless::fix::generated;
    //ASSERT_EQ(nullptr, dictionary(43, TokenMeta));
    const auto meta49 = dictionary(49, TokenMeta);
    //std::printf("meta: tag = %d, type = %d\n", meta49->tag, meta49->type);
}

TEST(Parser, Basics)
{
    using namespace org::limitless::fix::parser;

#define SOH "\x01"

    static constexpr uint8_t MESSAGE[] =
        "8=FIXT.1.1" SOH "9=118" SOH "35=A" SOH "49=Buyer" SOH "56=SellerSide" SOH "34=1" SOH
        "52=20190605-11:51:27.84800" SOH "1128=9" SOH "98=0" SOH "108=30" SOH "141=Y" SOH "553=Username" SOH
        "554=Password" SOH "1137=9" SOH "10=218" SOH
        // next message
        "8=FIXT.1.1" SOH "9=118" SOH;
    constexpr std::span buffer(MESSAGE, sizeof(MESSAGE) - 1);

    Parser parser{};

    auto [processed, status] = parser.parse(buffer, handler);
    ASSERT_EQ(ParserStatus::Success, status);
    // auto tok8 = parser.nextByTag(8);
    // ASSERT_TRUE(tok8 != nullptr && tok8->tag == 8);
    // auto tok9 = parser.nextByTag(9);
    // ASSERT_TRUE(tok9 != nullptr && tok9->tag == 9);
}

TEST(Parser, HopGroup)
{
    static constexpr uint8_t MESSAGE[] =
        "8=FIXT.1.1" SOH "9=152" SOH "35=A" SOH "49=Buyer" SOH "56=SellerSide" SOH "34=1" SOH
        "627=2" SOH "629=10" SOH "628=12" SOH "629=37" SOH "628=20" SOH
        "52=20190605-11:51:27.84800" SOH "1128=9" SOH "98=0" SOH "108=30" SOH "141=Y" SOH "553=Username" SOH
        "554=Password" SOH "1137=9" SOH "10=241" SOH
        // next message
        "8=FIXT.1.1" SOH "9=118" SOH;
    constexpr std::span buffer(MESSAGE, sizeof(MESSAGE) - 1);
    Parser parser{};

    auto [processed, status] = parser.parse(buffer, handler);
    ASSERT_EQ(ParserStatus::Success, status);

}
}
