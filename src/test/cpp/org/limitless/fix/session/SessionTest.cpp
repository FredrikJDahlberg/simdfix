//
// Created by Fredrik Dahlberg on 2026-06-25.
//

#include <gtest/gtest.h>

#include <span>
#include <type_traits>
#include <vector>

#include "org/limitless/fix/decoder/PayloadDecoder.hpp"
#include "org/limitless/fix/session/ClientSession.hpp"
#include "org/limitless/fix/session/ServerSession.hpp"
#include "org/limitless/fix/storage/MemoryStorage.hpp"

namespace org::limitless::fix::session
{

using namespace std::chrono;

#define SOH "\x01"

TEST(Session, BuilderProducesConcreteRole)
{
    using Client = ClientSession<FIXT_1_1, "CLIENT", "EXCH">;
    auto client = Client::Builder{NullStorage{}}.build();
    static_assert(std::is_same_v<decltype(client), Client>);
    EXPECT_EQ("CLIENT", client.senderCompId());
    EXPECT_EQ("EXCH", client.targetCompId());
    EXPECT_EQ(milliseconds{10000}, client.heartbeatPeriod());   // default constant

    using Server = ServerSession<FIXT_1_1, "EXCH", "CLIENT">;
    auto server = Server::Builder{NullStorage{}}
                      .heartbeatPeriod(milliseconds{20000})
                      .build();
    static_assert(std::is_same_v<decltype(server), Server>);
    EXPECT_EQ("EXCH", server.senderCompId());
    EXPECT_EQ("CLIENT", server.targetCompId());
    EXPECT_EQ(milliseconds{20000}, server.heartbeatPeriod());
}

TEST(Session, MemoryStorageBackedRole)
{
    using Client = ClientSession<FIXT_1_1, "CLIENT", "EXCH", storage::MemoryStorage>;

    auto client = Client::Builder{storage::MemoryStorage{}}
                      .heartbeatPeriod(milliseconds{30000})
                      .build();
    static_assert(std::is_same_v<decltype(client), Client>);
    EXPECT_EQ("CLIENT", client.senderCompId());
    EXPECT_EQ(milliseconds{30000}, client.heartbeatPeriod());

    using Server = ServerSession<FIXT_1_1, "EXCH", "CLIENT", storage::MemoryStorage>;
    auto server = Server::Builder{storage::MemoryStorage{}}.build();
    static_assert(std::is_same_v<decltype(server), Server>);
    EXPECT_EQ("EXCH", server.senderCompId());
}

struct CaptureTransport
{
    std::vector<uint8_t>* captured;
    void operator()(const std::span<const uint8_t> bytes) const
    {
        captured->assign(bytes.begin(), bytes.end());
    }
};

TEST(Session, SendRoutesEncodedMessageToTransport)
{
    std::vector<uint8_t> captured;
    auto client = ClientSession<FIXT_1_1, "SENDER", "TARGET", NullStorage, CaptureTransport>::Builder{NullStorage{}}
                      .transport(CaptureTransport{&captured})
                      .build();

    client.keepAlive(milliseconds{1'781'378'773'959});   // advance the session clock

    HeartbeatEncoder heartbeat{};
    client.wrapHeader(heartbeat);   // CompIDs from template, seqnum/time from session state
    client.send(heartbeat);         // finalize + hand bytes to the transport

    const std::string_view encoded{reinterpret_cast<const char*>(captured.data()), captured.size()};

    // BeginString (8) and CompIDs (49/56) come from the encoder's template
    // parameters; MsgSeqNum (34) and SendingTime (52) are stamped by wrapHeader.
    EXPECT_TRUE(encoded.starts_with("8=FIXT.1.1" SOH));
    EXPECT_NE(std::string_view::npos,
              encoded.find("49=SENDER" SOH "56=TARGET" SOH
                           "34=1" SOH "52=20260613-19:26:13.959" SOH));
}

// Standalone application handler — note it does NOT inherit from any session
// class; it is its own MessageHandler dispatcher and is injected by value.
class CaptureApplication : public MessageHandler<CaptureApplication>
{
public:
    using MessageHandler::handle;

    std::string symbol;
    uint32_t orderQty{};

    Result handle(NewOrderSingleDecoder& nos)
    {
        const auto sym = nos.symbol().value();
        symbol.assign(reinterpret_cast<const char*>(sym.data()), sym.size());
        orderQty = nos.orderQty().value();
        return Result::Success;
    }
};

static constexpr std::uint8_t NEW_ORDER_SINGLE[] =
    "8=FIXT.1.1" SOH "9=0129" SOH "35=D" SOH "49=SENDER" SOH "56=TARGET" SOH
    "34=1" SOH "52=20260613-19:26:13.959" SOH "11=ORDER1" SOH "21=1" SOH
    "55=AAPL" SOH "54=1" SOH "60=20260613-19:26:13.959" SOH "38=100" SOH
    "40=2" SOH "44=15000" SOH "10=126" SOH;

TEST(Session, ForwardsApplicationMessageToInjectedHandler)
{
    // The session owns the seven session messages; NewOrderSingle (35=D) is not
    // one of them, so the switch's default forwards it to the injected handler.
    using Server = ServerSession<FIXT_1_1, "TARGET", "SENDER", NullStorage, DiscardTransport, CaptureApplication>;
    auto server = Server::Builder{NullStorage{}}
                      .application(CaptureApplication{})
                      .build();

    org::limitless::fix::decoder::PayloadDecoder<FIXT_1_1> decoder;
    const Buffer buffer{NEW_ORDER_SINGLE, sizeof(NEW_ORDER_SINGLE) - 1};
    const auto [processed, status] = decoder.parse(buffer, server);

    EXPECT_EQ(Result::Success, status);
    EXPECT_EQ("AAPL", server.application().symbol);   // dispatched to the injected handler
    EXPECT_EQ(100u, server.application().orderQty);
}

}
