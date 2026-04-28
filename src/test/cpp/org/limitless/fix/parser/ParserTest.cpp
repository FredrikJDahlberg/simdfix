//
// Created by Fredrik Dahlberg on 2026-04-24.
//
#include <gtest/gtest.h>

#include "org/limitless/fix/parser/Decoder.hpp"
#include "org/limitless/fix/messages/Grammar.hpp"
#include "org/limitless/fix/messages/LogonDecoder.hpp"
#include "org/limitless/fix/messages/MessageHandler.hpp"

#define SOH "\x01"

namespace org::limitless::fix::parser {

using namespace org::limitless::fix::parser;

TEST(Parser, Logon)
{
    static constexpr uint8_t Logon[] =
        "8=FIXT.1.1" SOH "9=118" SOH "35=A" SOH "49=Buyer" SOH "56=SellerSide" SOH "34=1" SOH
        "52=20190605-11:51:27.84800" SOH "1128=9" SOH "98=0" SOH "108=30" SOH "141=Y" SOH "553=Username" SOH
        "554=Password" SOH "1137=9" SOH "10=218" SOH
        // next message
        "8=FIXT.1.1" SOH "9=118" SOH;
    constexpr std::span buffer(Logon, sizeof(Logon) - 1);

    struct AppHandler : generated::MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        ParserStatus handle(generated::LogonDecoder& logon)
        {
            std::printf("Found Logon\n");
            found = true;
            return ParserStatus::Success;
        }
    } app;

    Decoder parser{};
    auto [processed, status] = parser.parse(buffer, app);
    ASSERT_EQ(ParserStatus::Success, status);
    ASSERT_TRUE(app.found);
}

TEST(Parser, Logout)
{
    struct AppHandler : generated::MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        ParserStatus handle(generated::LogonDecoder& logon)
        {
            // skip
            return ParserStatus::Success;
        }

        ParserStatus handle(generated::LogoutDecoder& logout)
        {
            std::printf("Found Logout\n");
            found = true;
            return ParserStatus::Success;
        }
    } app;

    Decoder parser{};
    {
        static constexpr uint8_t Logout[] =
                "8=FIXT.1.1" SOH "9=34" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "10=178" SOH
                "               ";
        static constexpr std::span buffer(Logout, sizeof(Logout) - 1);
        auto [processed, status] = parser.parse(buffer, app);
        ASSERT_EQ(ParserStatus::Success, status);
        ASSERT_TRUE(app.found);
    }
    {
        static constexpr uint8_t Reject[] =
                "8=FIXT.1.1" SOH "9=12" SOH "35=3" SOH "45=666" SOH "10=107" SOH
                "               ";
        static constexpr std::span buffer(Reject, sizeof(Reject) - 1);
        auto [processed, status] = parser.parse(buffer, app);
        ASSERT_EQ(ParserStatus::InvalidMessageType, status);
    }

}

#if 0
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
    Decoder<generated::Grammar> parser{};

    AppHandler<generated::Grammar> handler{};
    auto [processed, status] = parser.parse(buffer, handler);
    //ASSERT_EQ(ParserStatus::Success, status);
}
#endif
}
