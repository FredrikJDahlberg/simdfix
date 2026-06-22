//
// Created by Fredrik Dahlberg on 2026-04-24.
//
#include <span>
#include <string>
#include <chrono>
#include <vector>

#include <gtest/gtest.h>

#include "org/limitless/fix/CodecTypes.hpp"
#include "org/limitless/fix/decoder/PayloadDecoder.hpp"
#include "org/limitless/fix/messages/FixMessageDecoders.hpp"
#include "org/limitless/fix/messages/FixMessageHandler.hpp"

#define SOH "\x01"

namespace org::limitless::fix::decoder {

using namespace org::limitless::fix::decoder;
using namespace org::limitless::fix::messages;

// Copies a message into an exact-size heap buffer so AddressSanitizer flags any
// read past the logical end. A string-literal span (makeSpan) leaves a readable
// '\0' immediately after the message, which would mask a one-byte tail over-read.
[[nodiscard]] inline std::vector<uint8_t> heap(const std::span<const uint8_t> source)
{
    return {source.begin(), source.end()};
}

TEST(MessageDecoder, Logon)
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
            const auto sender = logon.sender().value();
            EXPECT_EQ(std::string("Buyer"), std::string(reinterpret_cast<const char*>(sender.data()), sender.size()));
            EXPECT_EQ(Encryption::None, logon.encryptMethod().value());
            found = true;
            return Result::Success;
        }
    } app;

    PayloadDecoder<FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(login, app);
    ASSERT_EQ(Result::Success, status);
    ASSERT_TRUE(app.found);
}

TEST(MessageDecoder, Logout)
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

    PayloadDecoder<FIXT_1_1> decoder;
    {
        const auto logout = utils::makeSpan(
              "8=FIXT.1.1" SOH "9=84" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=10:11:12.123" SOH
              "627=2" SOH "629=10" SOH "628=12" SOH "629=37" SOH "628=20" SOH "10=211" SOH);
        auto [processed, status] = decoder.parse(logout, app);
        ASSERT_EQ(Result::Success, status);
        ASSERT_TRUE(app.found);
    }
    {
        const auto reject = utils::makeSpan("8=FIXT.1.1" SOH "9=41" SOH "35=Z" SOH
            "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "45=666" SOH "10=030" SOH);
        auto [processed, status] = decoder.parse(reject, app);
        ASSERT_EQ(Result::InvalidMessageType, status);
    }
}

TEST(MessageDecoder, MessageFragment)
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

    PayloadDecoder<FIXT_1_1> decoder;
    {
        const auto logout1 = utils::makeSpan(
              "8=FIXT.1.1" SOH "9=84" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH );
        auto [processed, status] = decoder.parse(logout1, app);
        ASSERT_EQ(Result::MessageFragment, status);
        ASSERT_EQ(0UL, processed);
        ASSERT_FALSE(app.found);
    }
    {
        const auto logout2 = utils::makeSpan(
              "8=FIXT.1.1" SOH "9=84" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH
              "34=100101" SOH "52=10:11:12.123" SOH "627=2" SOH "629=10" SOH
              "628=12" SOH "629=37" SOH "628=20" SOH "10=211" SOH);
        auto [processed, status] = decoder.parse(logout2, app);
        ASSERT_EQ(Result::Success, status);
        ASSERT_EQ(107UL, processed);
    }
}

TEST(MessageDecoder, HopGroup1)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(LogoutDecoder& logout)
        {
            std::printf("Got logout\n");
            auto& group = logout.hops();
            const auto count = group.count();
            EXPECT_EQ(2UL, count);
            group.next();
            {
                const auto v = group.hopCompID().value_or(String{});
                EXPECT_EQ(std::string{"12"}, std::string(v.data(), v.size()));
            }
            EXPECT_EQ(0UL, group.hopRefID().value_or(0));
            EXPECT_TRUE(group.hasNext());
            group.next();
            EXPECT_EQ(0UL, group.hopRefID().value_or(0));
            {
                const auto v = group.hopCompID().value_or(std::string{});
                EXPECT_EQ(std::string{"20"}, std::string(v.data(), v.size()));
            }
            EXPECT_FALSE(group.hasNext());
            return Result::Success;
        }
    } app;
    PayloadDecoder<FIXT_1_1> decoder;
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

TEST(MessageDecoder, HopGroup2)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(LogoutDecoder& logout)
        {
            std::printf("Got logout\n");
            auto& group = logout.hops();
            const auto count = group.count();
            std::printf("Group hops=%d\n", count);
            EXPECT_EQ(2UL, count);
            group.next();
            std::string empty{};
            EXPECT_EQ(empty, group.hopCompID().value_or(empty));
            EXPECT_EQ(0UL, group.hopRefID().value_or(0));
            EXPECT_TRUE(group.hasNext());
            group.next();
            EXPECT_EQ("20", toString(group.hopCompID().value()));
            EXPECT_FALSE(group.hasNext());
            return Result::Success;
        }
    } app;
    PayloadDecoder<FIXT_1_1> decoder;
    const auto logout = utils::makeSpan("8=FIXT.1.1" SOH "9=86" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH
        "52=20260609-12:13:14.000" SOH "34=100101" SOH "627=2" SOH "629=10" SOH "629=37" SOH "628=20" SOH  "10=090" SOH);
    auto[processed, status] = decoder.parse(logout, app);
    ASSERT_EQ(Result::Success, status);
}

TEST(MessageDecoder, HopGroup3)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(LogoutDecoder& logout)
        {
            std::printf("Got logout\n");
            auto& group = logout.hops();
            const auto count = group.count();//.value_or(0);
            std::printf("Group hops=%d\n", count);
            EXPECT_EQ(2UL, count);
            group.next();
            std::string empty{};
            EXPECT_EQ(empty, group.hopCompID().value_or(empty));
            EXPECT_EQ(utils::dateTimeToEpochUTC(std::string_view("20260609-12:13:14.000")),
                      group.hopSendingTime().value_or(std::chrono::milliseconds{0}));
            EXPECT_TRUE(group.hasNext());
            group.next();
            EXPECT_EQ(utils::dateTimeToEpochUTC(std::string_view("20260609-12:13:15.000")),
                      group.hopSendingTime().value_or(std::chrono::milliseconds{0}));
            EXPECT_EQ(empty, group.hopCompID().value_or(empty));
            EXPECT_FALSE(group.hasNext());
            return Result::Success;
        }
    } app;
    PayloadDecoder<FIXT_1_1> decoder;
    const auto logout = utils::makeSpan("8=FIXT.1.1" SOH "9=108" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH
        "52=12:13:14.000" SOH "34=100101" SOH "627=2" SOH "629=20260609-12:13:14.000" SOH
        "629=20260609-12:13:15.000" SOH "10=253" SOH);
    auto[processed, status] = decoder.parse(logout, app);
    ASSERT_EQ(Result::Success, status);
}

TEST(MessageDecoder, InvalidGroupCount)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(LogoutDecoder& logout)
        {
            std::printf("Got logout\n");
            auto& group = logout.hops();
            const auto count = group.count();
            std::printf("Group hops=%d\n", count);
            EXPECT_EQ(2UL, count);
            group.next();
            EXPECT_EQ("20", toString(group.hopCompID().value()));
            //EXPECT_EQ(10, group.hopRefID().value_or(0));
            EXPECT_TRUE(group.hasNext());
            group.next();
            EXPECT_FALSE(group.hasNext());
            return Result::Success;
        }
    } app;
    PayloadDecoder<FIXT_1_1> decoder;
    const auto logout = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=70" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=12:12:12.123" SOH
        "627=2" SOH "629=10" SOH "628=20" SOH "10=071" SOH);
    auto[processed, status] = decoder.parse(logout, app);
    ASSERT_EQ(Result::Success, status);
}

TEST(MessageDecoder, NewOrderSingle)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(NewOrderSingleDecoder& order)
        {
            EXPECT_EQ("ORDER1", toString(order.clOrdID().value()));
            EXPECT_EQ(HandlInst::AutoPrivate, order.handlInst().value());
            EXPECT_EQ("AAPL", toString(order.symbol().value()));
            EXPECT_EQ(Side::Buy, order.side().value());
            EXPECT_EQ(100U, order.orderQty().value());
            EXPECT_EQ(OrdType::Limit, order.ordType().value());
            EXPECT_EQ(utils::FixedDecimal(15000, 0), order.price().value());
            found = true;
            return Result::Success;
        }
    } app;

    PayloadDecoder<FIXT_1_1> decoder;
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0129" SOH "35=D" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=1" SOH "52=20260613-19:26:13.959" SOH
        "11=ORDER1" SOH "21=1" SOH "55=AAPL" SOH "54=1" SOH "60=20260613-19:26:13.959" SOH
        "38=100" SOH "40=2" SOH "44=15000" SOH "10=126" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::Success, status);
    ASSERT_TRUE(app.found);
}

TEST(MessageDecoder, NewOrderSingleWithDateAndTime)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(NewOrderSingleDecoder& order)
        {
            EXPECT_EQ("ORDER2", toString(order.clOrdID().value()));
            EXPECT_EQ("AAPL", toString(order.symbol().value()));
            EXPECT_EQ(Side::Buy, order.side().value());
            EXPECT_EQ(100U, order.orderQty().value());
            EXPECT_EQ(OrdType::Limit, order.ordType().value());

            const auto tradeDate = order.tradeDate();
            EXPECT_TRUE(tradeDate.has_value());
            EXPECT_EQ(std::chrono::milliseconds{1'781'308'800'000}, tradeDate.value());

            const auto maturityTime = order.maturityTime();
            EXPECT_TRUE(maturityTime.has_value());
            EXPECT_EQ(std::chrono::milliseconds{45'296'789}, maturityTime.value());

            found = true;
            return Result::Success;
        }
    } app;

    PayloadDecoder<FIXT_1_1> decoder;
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0166" SOH "35=D" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=1" SOH "52=20260613-19:26:13.959" SOH
        "11=ORDER2" SOH "21=1" SOH "55=AAPL" SOH "54=1" SOH "60=20260613-19:26:13.959" SOH
        "38=100" SOH "40=2" SOH "44=120.00000000" SOH
        "75=20260613" SOH "1079=12:34:56.789" SOH "10=151" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::Success, status);
    ASSERT_TRUE(app.found);
}

TEST(MessageDecoder, LogonWithXmlData)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(LogonDecoder& logon)
        {
            EXPECT_EQ(Encryption::None, logon.encryptMethod().value());
            EXPECT_EQ(30U, logon.heartbeatInterval().value());
            const auto data = logon.xmlData().get();
            EXPECT_TRUE(data.has_value());
            EXPECT_EQ(11U, data.value().size());
            EXPECT_EQ(std::string_view("<root/>test"),
                      std::string_view(reinterpret_cast<const char*>(data.value().data()), data.value().size()));
            found = true;
            return Result::Success;
        }
    } app;

    PayloadDecoder<FIXT_1_1> decoder;
    const auto message = heap(utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0090" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=1" SOH "52=20260613-19:26:13.959" SOH "98=0" SOH "108=30" SOH
        "212=11" SOH "213=<root/>test" SOH "10=124" SOH));
    auto [processed, status] = decoder.parse(Buffer{message.data(), message.size()}, app);
    ASSERT_EQ(Result::Success, status);
    ASSERT_TRUE(app.found);
}

TEST(MessageDecoder, LogonWithXmlDataEmbeddedSoh)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(LogonDecoder& logon)
        {
            EXPECT_EQ(Encryption::None, logon.encryptMethod().value());
            EXPECT_EQ(30U, logon.heartbeatInterval().value());
            const auto data = logon.xmlData().get();
            EXPECT_TRUE(data.has_value());
            EXPECT_EQ(12U, data.value().size());
            const std::array<uint8_t, 12> expected =
            {
                '<', 'r', 'o', 'o', 't', 0x01, '/', '>', 't', 'e', 's', 't'
            };
            EXPECT_TRUE(std::ranges::equal(data.value(), expected));
            found = true;
            return Result::Success;
        }
    } app;

    PayloadDecoder<FIXT_1_1> decoder;
    const auto message = heap(utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0091" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=1" SOH "52=20260613-19:26:13.959" SOH "98=0" SOH "108=30" SOH
        "212=12" SOH "213=<root" SOH "/>test" SOH "10=127" SOH));
    auto [processed, status] = decoder.parse(Buffer{message.data(), message.size()}, app);
    ASSERT_EQ(Result::Success, status);
    ASSERT_TRUE(app.found);
}

TEST(MessageDecoder, LogonWithXmlDataInlineSkip)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(LogonDecoder& logon)
        {
            EXPECT_EQ(Encryption::None, logon.encryptMethod().value());
            EXPECT_EQ(30U, logon.heartbeatInterval().value());
            const auto data = logon.xmlData().get();
            EXPECT_TRUE(data.has_value());
            EXPECT_EQ(12U, data.value().size());
            const std::array<uint8_t, 12> expected =
            {
                '<', 'r', 'o', 'o', 't', 0x01, '/', '>', 't', 'e', 's', 't'
            };
            EXPECT_TRUE(std::ranges::equal(data.value(), expected));
            found = true;
            return Result::Success;
        }
    } app;

    struct DataFields
    {
        static constexpr int32_t dataTag(const uint16_t tag)
        {
            if (tag == 212) return 213;
            return -1;
        }
    };
    PayloadDecoder<FIXT_1_1, DataFields> decoder;
    const auto message = heap(utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0091" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=1" SOH "52=20260613-19:26:13.959" SOH "98=0" SOH "108=30" SOH
        "212=12" SOH "213=<root" SOH "/>test" SOH "10=127" SOH));
    auto [processed, status] = decoder.parse(Buffer{message.data(), message.size()}, app);
    ASSERT_EQ(Result::Success, status);
    ASSERT_TRUE(app.found);

    const auto fields = decoder.fields();
    for (size_t i = 0; i < fields.size(); ++i)
    {
        if (fields[i].m_tag == 213)
        {
            EXPECT_EQ(12U, fields[i].m_length);
            break;
        }
    }
}

TEST(MessageDecoder, ResendRequest)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(ResendRequestDecoder& resend)
        {
            EXPECT_EQ(1U, resend.beginSeqNo().value());
            EXPECT_EQ(4U, resend.endSeqNo().value());
            EXPECT_EQ(5U, resend.sequenceNumber().value());
            found = true;
            return Result::Success;
        }
    } app;

    PayloadDecoder<FIXT_1_1> decoder;
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0064" SOH "35=2" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=5" SOH "52=20260613-19:26:13.959" SOH "7=1" SOH "16=4" SOH "10=162" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::Success, status);
    ASSERT_TRUE(app.found);
}

TEST(MessageDecoder, Reject)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(RejectDecoder& reject)
        {
            EXPECT_EQ(9U, reject.refSeqNum().value());
            EXPECT_EQ(55U, reject.refTagID().value());
            EXPECT_EQ(MessageType::NewOrderSingle, reject.refMsgType().value());
            EXPECT_EQ(SessionRejectReason::RequiredTagMissing, reject.sessionRejectReason().value());
            EXPECT_EQ("Missing Symbol", toString(reject.text().value()));
            found = true;
            return Result::Success;
        }
    } app;

    PayloadDecoder<FIXT_1_1> decoder;
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0098" SOH "35=3" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=10" SOH "52=20260613-19:26:13.959" SOH "45=9" SOH "371=55" SOH "372=D" SOH
        "373=1" SOH "58=Missing Symbol" SOH "10=191" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::Success, status);
    ASSERT_TRUE(app.found);
}

TEST(MessageDecoder, RejectMinimal)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(RejectDecoder& reject)
        {
            EXPECT_EQ(2U, reject.refSeqNum().value());
            EXPECT_FALSE(reject.refTagID().has_value());
            EXPECT_FALSE(reject.refMsgType().has_value());
            EXPECT_FALSE(reject.sessionRejectReason().has_value());
            EXPECT_FALSE(reject.text().has_value());
            found = true;
            return Result::Success;
        }
    } app;

    PayloadDecoder<FIXT_1_1> decoder;
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0060" SOH "35=3" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=3" SOH "52=20260613-19:26:13.959" SOH "45=2" SOH "10=247" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::Success, status);
    ASSERT_TRUE(app.found);
}

TEST(MessageDecoder, SequenceReset)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(SequenceResetDecoder& seqReset)
        {
            EXPECT_EQ(GapFillFlag::GapFillMessage, seqReset.gapFillFlag().value());
            EXPECT_EQ(10U, seqReset.newSeqNo().value());
            found = true;
            return Result::Success;
        }
    } app;

    PayloadDecoder<FIXT_1_1> decoder;
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0067" SOH "35=4" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=5" SOH "52=20260613-19:26:13.959" SOH "123=Y" SOH "36=10" SOH "10=093" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::Success, status);
    ASSERT_TRUE(app.found);
}

TEST(MessageDecoder, SequenceResetNoGapFill)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(SequenceResetDecoder& seqReset)
        {
            EXPECT_FALSE(seqReset.gapFillFlag().has_value());
            EXPECT_EQ(10U, seqReset.newSeqNo().value());
            found = true;
            return Result::Success;
        }
    } app;

    PayloadDecoder<FIXT_1_1> decoder;
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0061" SOH "35=4" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=5" SOH "52=20260613-19:26:13.959" SOH "36=10" SOH "10=042" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::Success, status);
    ASSERT_TRUE(app.found);
}

TEST(MessageDecoder, ExecutionReportFill)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(ExecutionReportDecoder& report)
        {
            EXPECT_EQ("ORD001", toString(report.orderID().value()));
            EXPECT_EQ("CL001", toString(report.clOrdID().value()));
            EXPECT_EQ("EXEC001", toString(report.execID().value()));
            EXPECT_EQ(ExecType::Trade, report.execType().value());
            EXPECT_EQ(OrdStatus::Filled, report.ordStatus().value());
            EXPECT_EQ("AAPL", toString(report.symbol().value()));
            EXPECT_EQ(Side::Buy, report.side().value());
            EXPECT_EQ(100U, report.orderQty().value());
            EXPECT_EQ(utils::FixedDecimal(15050, -2), report.price().value());
            EXPECT_EQ(100U, report.lastQty().value());
            EXPECT_EQ(utils::FixedDecimal(15050, -2), report.lastPx().value());
            EXPECT_EQ(0U, report.leavesQty().value());
            EXPECT_EQ(100U, report.cumQty().value());
            EXPECT_EQ(utils::FixedDecimal(15050, -2), report.avgPx().value());
            found = true;
            return Result::Success;
        }
    } app;

    PayloadDecoder<FIXT_1_1> decoder;
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0208" SOH "35=8" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=3" SOH "52=20260613-19:26:13.959" SOH
        "37=ORD001" SOH "11=CL001" SOH "17=EXEC001" SOH "150=F" SOH "39=2" SOH
        "55=AAPL" SOH "54=1" SOH "38=100" SOH "44=150.50000000" SOH
        "32=100" SOH "31=150.50000000" SOH "151=0" SOH "14=100" SOH
        "6=150.50000000" SOH "60=20260613-19:26:13.959" SOH "10=023" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::Success, status);
    ASSERT_TRUE(app.found);
}

TEST(MessageDecoder, ExecutionReportMinimal)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(ExecutionReportDecoder& report)
        {
            EXPECT_EQ("ORD002", toString(report.orderID().value()));
            EXPECT_FALSE(report.clOrdID().has_value());
            EXPECT_EQ("EXEC002", toString(report.execID().value()));
            EXPECT_EQ(ExecType::New, report.execType().value());
            EXPECT_EQ(OrdStatus::New, report.ordStatus().value());
            EXPECT_EQ("MSFT", toString(report.symbol().value()));
            EXPECT_EQ(Side::Sell, report.side().value());
            EXPECT_FALSE(report.orderQty().has_value());
            EXPECT_FALSE(report.price().has_value());
            EXPECT_FALSE(report.lastQty().has_value());
            EXPECT_FALSE(report.lastPx().has_value());
            EXPECT_EQ(200U, report.leavesQty().value());
            EXPECT_EQ(0U, report.cumQty().value());
            EXPECT_FALSE(report.text().has_value());
            found = true;
            return Result::Success;
        }
    } app;

    PayloadDecoder<FIXT_1_1> decoder;
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0142" SOH "35=8" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=1" SOH "52=20260613-19:26:13.959" SOH
        "37=ORD002" SOH "17=EXEC002" SOH "150=0" SOH "39=0" SOH
        "55=MSFT" SOH "54=2" SOH "151=200" SOH "14=0" SOH
        "6=0" SOH "60=20260613-19:26:13.959" SOH "10=249" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::Success, status);
    ASSERT_TRUE(app.found);
}

TEST(MessageDecoder, ExecutionReportReject)
{
    struct AppHandler : MessageHandler<AppHandler>
    {
        using MessageHandler::handle;

        bool found = false;

        Result::Values handle(ExecutionReportDecoder& report)
        {
            EXPECT_EQ("NONE", toString(report.orderID().value()));
            EXPECT_EQ("CL003", toString(report.clOrdID().value()));
            EXPECT_EQ("EXEC003", toString(report.execID().value()));
            EXPECT_EQ(ExecType::Rejected, report.execType().value());
            EXPECT_EQ(OrdStatus::Rejected, report.ordStatus().value());
            EXPECT_EQ("TSLA", toString(report.symbol().value()));
            EXPECT_EQ(Side::Buy, report.side().value());
            EXPECT_EQ(0U, report.leavesQty().value());
            EXPECT_EQ(0U, report.cumQty().value());
            EXPECT_EQ("Insufficient buying power", toString(report.text().value()));
            found = true;
            return Result::Success;
        }
    } app;

    PayloadDecoder<FIXT_1_1> decoder;
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0176" SOH "35=8" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=5" SOH "52=20260613-19:26:13.959" SOH
        "37=NONE" SOH "11=CL003" SOH "17=EXEC003" SOH "150=8" SOH "39=8" SOH
        "55=TSLA" SOH "54=1" SOH "151=0" SOH "14=0" SOH
        "6=0" SOH "60=20260613-19:26:13.959" SOH
        "58=Insufficient buying power" SOH "10=180" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::Success, status);
    ASSERT_TRUE(app.found);
}

TEST(MessageDecoder, InvalidBeginString)
{
    PayloadDecoder<FIXT_1_1> decoder;
    struct AppHandler : MessageHandler<AppHandler>{} app;
    // "FIX.4.2" does not match the PayloadDecoder<FIXT_1_1> expected prefix
    const auto message = utils::makeSpan(
        "8=FIX.4.2" SOH "9=0053" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=1" SOH "52=20260613-19:26:13.959" SOH "10=000" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::InvalidBeginString, status) << Result(status).name();
}

TEST(MessageDecoder, InvalidCheckSum)
{
    // Use a valid Logon from the encoder test, but replace the checksum value
    PayloadDecoder<FIXT_1_1> decoder;
    struct AppHandler : MessageHandler<AppHandler>{} app;
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0067" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=1" SOH "52=20260613-19:26:13.959" SOH "98=0" SOH "108=30" SOH "10=999" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::InvalidCheckSum, status) << Result(status).name();
}

TEST(MessageDecoder, InvalidMessageType)
{
    // Valid structure but MsgType 'Z' is not registered in the handler
    PayloadDecoder<FIXT_1_1> decoder;
    struct AppHandler : MessageHandler<AppHandler>{} app;
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0067" SOH "35=Z" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=1" SOH "52=20260613-19:26:13.959" SOH "98=0" SOH "108=30" SOH "10=099" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::InvalidMessageType, status) << Result(status).name();
}

TEST(MessageDecoder, InvalidSenderCompId)
{
    // Valid Logon but SenderCompID "WRONG!" does not match the session context
    PayloadDecoder<FIXT_1_1> decoder;
    SessionContext context{"FIXT.1.1", "SENDER", "TARGET"};

    struct AppHandler : MessageHandler<AppHandler>{} app;
    app.setSessionContext(context);
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0067" SOH "35=A" SOH "49=WRONG!" SOH "56=TARGET" SOH
        "34=1" SOH "52=20260613-19:26:13.959" SOH "98=0" SOH "108=30" SOH "10=055" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::InvalidSenderCompId, status) << Result(status).name();
}

TEST(MessageDecoder, InvalidTargetCompId)
{
    // Valid Logon but TargetCompID "WRONG!" does not match the session context
    PayloadDecoder<FIXT_1_1> decoder;
    SessionContext context{"FIXT.1.1", "SENDER", "TARGET"};

    struct AppHandler : MessageHandler<AppHandler>{} app;
    app.setSessionContext(context);
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0067" SOH "35=A" SOH "49=SENDER" SOH "56=WRONG!" SOH
        "34=1" SOH "52=20260613-19:26:13.959" SOH "98=0" SOH "108=30" SOH "10=049" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::InvalidTargetCompId, status) << Result(status).name();
}

TEST(MessageDecoder, MissingSendingTime)
{
    // Valid structure but missing tag 52 (SendingTime)
    PayloadDecoder<FIXT_1_1> decoder;
    struct AppHandler : MessageHandler<AppHandler>{} app;
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0042" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=1" SOH "98=0" SOH "108=30" SOH "10=094" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::InvalidSendingTime, status) << Result(status).name();
}

TEST(MessageDecoder, MissingSequenceNumber)
{
    // Valid structure but missing tag 34 (MsgSeqNum)
    PayloadDecoder<FIXT_1_1> decoder;
    struct AppHandler : MessageHandler<AppHandler>{} app;
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0062" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH
        "52=20260613-19:26:13.959" SOH "98=0" SOH "108=30" SOH "10=111" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::InvalidSequenceNumber, status) << Result(status).name();
}

TEST(MessageDecoder, MissingRequiredField)
{
    // Logon without HeartbeatInterval (tag 108) — required by the protocol
    PayloadDecoder<FIXT_1_1> decoder;
    struct AppHandler : MessageHandler<AppHandler>{} app;
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=0060" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH
        "34=1" SOH "52=20260613-19:26:13.959" SOH "98=0" SOH "10=009" SOH);
    auto [processed, status] = decoder.parse(message, app);
    ASSERT_EQ(Result::RequiredFieldMissing, status) << Result(status).name();
}

TEST(MessageDecoder, InvalidUTCTimestamp)
{
    // Logon with invalid SendingTime (hour 25)
    {
        PayloadDecoder<FIXT_1_1> decoder;
        struct AppHandler : MessageHandler<AppHandler>
        {
            using MessageHandler::handle;

            Result::Values handle(LogonDecoder& logon)
            {
                EXPECT_FALSE(logon.sendingTime().has_value());
                EXPECT_EQ(Result::InvalidLength, logon.sendingTime().error());
                return Result::Success;
            }
        } app;
        const auto message = utils::makeSpan(
            "8=FIXT.1.1" SOH "9=0067" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH
            "34=1" SOH "52=20260613-25:26:13.959" SOH "98=0" SOH "108=30" SOH "10=071" SOH);
        auto [processed, status] = decoder.parse(message, app);
        ASSERT_EQ(Result::Success, status);
    }
    // NOS with invalid TransactTime (hour 25), invalid DateOnly (month 13),
    // and invalid TimeOnly (minute 60)
    {
        PayloadDecoder<FIXT_1_1> decoder;
        struct AppHandler : MessageHandler<AppHandler>
        {
            using MessageHandler::handle;

            Result::Values handle(NewOrderSingleDecoder& order)
            {
                EXPECT_FALSE(order.transactTime().has_value());
                EXPECT_EQ(Result::InvalidLength, order.transactTime().error());
                EXPECT_FALSE(order.tradeDate().has_value());
                EXPECT_EQ(Result::InvalidLength, order.tradeDate().error());
                EXPECT_FALSE(order.maturityTime().has_value());
                EXPECT_EQ(Result::InvalidLength, order.maturityTime().error());
                return Result::Success;
            }
        } app;
        const auto message = utils::makeSpan(
            "8=FIXT.1.1" SOH "9=0159" SOH "35=D" SOH "49=SENDER" SOH "56=TARGET" SOH
            "34=1" SOH "52=20260613-19:26:13.959" SOH
            "11=ORDER1" SOH "21=1" SOH "55=AAPL" SOH "54=1" SOH "60=20260613-25:00:00.000" SOH
            "38=100" SOH "40=2" SOH "44=15000" SOH
            "75=20261301" SOH "1079=12:60:00.000" SOH "10=254" SOH);
        auto [processed, status] = decoder.parse(message, app);
        ASSERT_EQ(Result::Success, status);
    }
}

TEST(MessageDecoder, InvalidMandatoryFields)
{
    PayloadDecoder<FIXT_1_1> decoder;
    struct AppHandler : MessageHandler<AppHandler>{} app;
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
