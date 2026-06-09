//
// Created by Fredrik Dahlberg on 2026-04-24.
//
#include <span>
#include <string>

#include <gtest/gtest.h>

#include "org/limitless/fix/decoder/PayloadDecoder.hpp"
#include "org/limitless/fix/messages/MessageDecoders.hpp"
#include "org/limitless/fix/messages/MessageHandler.hpp"

#define SOH "\x01"

namespace org::limitless::fix::decoder {

using namespace org::limitless::fix::decoder;
using namespace org::limitless::fix::generated;

TEST(Parser, Logon)
{
    const auto login = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=118" SOH "35=A" SOH "49=Buyer" SOH "56=SellerSide" SOH "34=1" SOH
        "52=20190605-11:51:27.84800" SOH "1128=9" SOH "98=0" SOH "108=30" SOH "141=Y" SOH "553=Username" SOH
        "554=Password" SOH "1137=9" SOH "10=218" SOH
        // next message
        "8=FIXT.1.1" SOH "9=118" SOH
    );
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(LogonDecoder& logon)
        {
            const auto sender = logon.standardHeader().sender().value();
            EXPECT_EQ(std::string("Buyer"), std::string(reinterpret_cast<const char*>(sender.data()), sender.size()));
            EXPECT_EQ(Encryption::None, logon.encryptMethod().value().m_value);
            found = true;
            return Result::Success;
        }
    } app;

    PayloadDecoder decoder{};
    auto [processed, status] = decoder.parse(login, app);
    ASSERT_EQ(Result::Success, status);
    ASSERT_TRUE(app.found);
}

TEST(Parser, Logout)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        // skip logon

        Result::Values handle(LogoutDecoder&)
        {
            std::printf("Found Logout\n");
            found = true;
            return Result::Success;
        }
    } app;

    PayloadDecoder decoder{};
    {
        const auto logout = utils::makeSpan(
              "8=FIXT.1.1" SOH "9=84" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=10:11:12.123" SOH
              "627=2" SOH "629=10" SOH "628=12" SOH "629=37" SOH "628=20" SOH "10=211" SOH);
        auto [processed, status] = decoder.parse(logout, app);
        ASSERT_EQ(Result::Success, status);
        ASSERT_TRUE(app.found);
    }
    {
        const auto reject = utils::makeSpan("8=FIXT.1.1" SOH "9=41" SOH "35=3" SOH
            "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "45=666" SOH "10=247" SOH);
        auto [processed, status] = decoder.parse(reject, app);
        ASSERT_EQ(Result::InvalidMessageType, status);
    }
}

TEST(Parser, MessageFragment)
{
    struct AppHandler : generated::MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        // skip logon

        Result::Values handle(LogoutDecoder&)
        {
            std::printf("Found Logout\n");
            found = true;
            return Result::Success;
        }
    } app;

    PayloadDecoder decoder{};
    {
        const auto logout1 = utils::makeSpan(
              "8=FIXT.1.1" SOH "9=84" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH );
        auto [processed, status] = decoder.parse(logout1, app);
        ASSERT_EQ(Result::MessageFragment, status);
        ASSERT_EQ(0, processed);
        ASSERT_FALSE(app.found);
    }
    {
        const auto logout2 = utils::makeSpan(
              "8=FIXT.1.1" SOH "9=84" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH
              "34=100101" SOH "52=10:11:12.123" SOH "627=2" SOH "629=10" SOH
              "628=12" SOH "629=37" SOH "628=20" SOH "10=211" SOH);
        auto [processed, status] = decoder.parse(logout2, app);
        ASSERT_EQ(Result::Success, status);
        ASSERT_EQ(107, processed);
    }
}

TEST(Parser, HopGroup1)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(LogoutDecoder& logout)
        {
            std::printf("Got logout\n");
            auto group = logout.standardHeader().hops();
            const auto count = group.count();
            EXPECT_EQ(2, count);
            group.next();
            {
                const auto v = group.hopCompID().value_or(std::span<const uint8_t>{});
                EXPECT_EQ(std::string("12"), std::string(reinterpret_cast<const char*>(v.data()), v.size()));
            }
            EXPECT_EQ(0, group.hopRefID().value_or(0));
            EXPECT_TRUE(group.hasNext());
            group.next();
            EXPECT_EQ(0, group.hopRefID().value_or(0));
            {
                const auto v = group.hopCompID().value_or(std::span<const uint8_t>{});
                EXPECT_EQ(std::string("12"), std::string(reinterpret_cast<const char*>(v.data()), v.size()));
            }
            EXPECT_FALSE(group.hasNext());
            return Result::Success;
        }
    } app;
    PayloadDecoder decoder{};
    const auto logout = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=84" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=10:11:12.123" SOH
        "627=2" SOH "629=10" SOH "628=12" SOH "629=37" SOH "628=20" SOH "10=211" SOH);
    auto[processed, status] = decoder.parse(logout, app);
    ASSERT_EQ(Result::Success, status);
}


template <typename Span>
std::string toString(Span span)
{
    return std::string(reinterpret_cast<const char*>(span.data()), span.size());
}

TEST(Parser, HopGroup2)
{
    struct AppHandler : generated::MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(generated::LogoutDecoder& logout)
        {
            std::printf("Got logout\n");
            auto group = logout.standardHeader().hops();
            const auto count = group.count();
            std::printf("Group hops=%d\n", count);
            EXPECT_EQ(2, count);
            group.next();
            EXPECT_EQ("", toString(group.hopCompID().value()));
            EXPECT_EQ(0, group.hopRefID().value_or(0));
            EXPECT_TRUE(group.hasNext());
            group.next();
            EXPECT_EQ("20", toString(group.hopCompID().value()));
            EXPECT_FALSE(group.hasNext());
            return Result::Success;
        }
    } app;
    PayloadDecoder decoder{};
    const auto logout = utils::makeSpan("8=FIXT.1.1" SOH "9=86" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH
        "52=20260609-12:13:14.000" SOH "34=100101" SOH "627=2" SOH "629=10" SOH "629=37" SOH "628=20" SOH  "10=090" SOH);
    auto[processed, status] = decoder.parse(logout, app);
    ASSERT_EQ(Result::Success, status);
}

TEST(Parser, HopGroup3)
{
    struct AppHandler : generated::MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(generated::LogoutDecoder& logout)
        {
            std::printf("Got logout\n");
            auto group = logout.standardHeader().hops();
            const auto count = group.count();//.value_or(0);
            std::printf("Group hops=%d\n", count);
            EXPECT_EQ(2, count);
            group.next();
            // FIXME: string EXPECT_EQ(0, group.hopCompID().value_or(0));
            EXPECT_EQ(10, group.hopRefID().value_or(0));
            EXPECT_TRUE(group.hasNext());
            group.next();
            EXPECT_EQ(37, group.hopRefID().value_or(0));
            // FIXME string EXPECT_EQ(0, group.hopCompID().value_or(0));
            EXPECT_FALSE(group.hasNext());
            return Result::Success;
        }
    } app;
    PayloadDecoder decoder{};
    const auto logout = utils::makeSpan("8=FIXT.1.1" SOH "9=70" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH
        "52=12:13:14.000" SOH "34=100101" SOH "627=2" SOH "629=10" SOH "629=37" SOH "10=077" SOH);
    auto[processed, status] = decoder.parse(logout, app);
    ASSERT_EQ(Result::Success, status);
}

TEST(Parser, InvalidGroupCount)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(generated::LogoutDecoder& logout)
        {
            std::printf("Got logout\n");
            auto group = logout.standardHeader().hops();
            const auto count = group.count();
            std::printf("Group hops=%d\n", count);
            EXPECT_EQ(2, count);
            group.next();
            EXPECT_EQ("20", toString(group.hopCompID().value()));
            //EXPECT_EQ(10, group.hopRefID().value_or(0));
            EXPECT_TRUE(group.hasNext());
            group.next();
            EXPECT_FALSE(group.hasNext());
            return Result::Success;
        }
    } app;
    PayloadDecoder decoder{};
    const auto logout = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=70" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=12:12:12.123" SOH
        "627=2" SOH "629=10" SOH "628=20" SOH "10=071" SOH);
    auto[processed, status] = decoder.parse(logout, app);
    ASSERT_EQ(Result::Success, status);
}

TEST(Parser, InvalidMandatoryFields)
{
    PayloadDecoder decoder{};
    struct AppHandler : generated::MessageHandler<AppHandler>{} app;
    {
        const auto message = utils::makeSpan("666=FIXT.1.1" SOH);
        auto[processed, status] = decoder.parse(message, app);
        ASSERT_EQ(Result::MessageFragment, status) << Result(status).name();
    }
    {
        const auto message = utils::makeSpan("8=FIXT.1.1" SOH "666=66" SOH "666=66" SOH "666=66" SOH);
        auto[processed, status] = decoder.parse(message, app);
        ASSERT_EQ(Result::InvalidBodyLengthTag, status) << Result(status).name();
    }
    {
        const auto message = utils::makeSpan("8=FIXT.1.1" SOH "9=35" SOH "52=101112.123" SOH
            "666=66" SOH "666=66" SOH "666=66" SOH "10=043" SOH);
        auto[processed, status] = decoder.parse(message, app);
        ASSERT_EQ(Result::InvalidMessageTypeTag, status) << Result(status).name();
    }
    {
        const auto message = utils::makeSpan("8=FIXT.1.1" SOH "666=66" SOH "35=66" SOH "666=66" SOH
            "666=66" SOH "666=66" SOH "10=043" SOH);
        auto[processed, status] = decoder.parse(message, app);
        ASSERT_EQ(Result::InvalidBodyLengthTag, status) << Result(status).name();
    }
    {
        const auto message = utils::makeSpan("8=FIXT.1.1" SOH "9=666" SOH "35=66" SOH "666=66" SOH
            "666=66" SOH "666=66" SOH "10=043" SOH);
        auto[processed, status] = decoder.parse(message, app);
        ASSERT_EQ(Result::InvalidBodyLength, status) << Result(status).name();
    }
    {
        const auto message = utils::makeSpan("8=FIXT.1.1" SOH "9=28" SOH "666=66" SOH "666=66" SOH
            "666=66" SOH "666=66" SOH "10=063" SOH);
        auto[processed, status] = decoder.parse(message, app);
        ASSERT_EQ(Result::InvalidMessageTypeTag, status) << Result(status).name();
    }
    {
        const auto message = utils::makeSpan("8=FIXT.1.1" SOH "9=48" SOH "35=66" SOH "666=66" SOH
            "666=66" SOH "666=66" SOH "11=043" SOH "                     ");
        auto[processed, status] = decoder.parse(message, app);
        ASSERT_EQ(Result::InvalidCheckSumTag, status) << Result(status).name();
    }
}

}


