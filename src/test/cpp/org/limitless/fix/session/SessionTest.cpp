//
// Created by Fredrik Dahlberg on 2026-06-25.
//

#include <gtest/gtest.h>

#include <array>
#include <type_traits>

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

TEST(Session, IsSessionMessageClassifiesAdminMessages)
{
    using Client = ClientSession<FIXT_1_1, "CLIENT", "EXCH">;
    EXPECT_TRUE(Client::isSessionMessage(LogonDecoder::MessageId));
    EXPECT_TRUE(Client::isSessionMessage(LogoutDecoder::MessageId));
    EXPECT_TRUE(Client::isSessionMessage(HeartbeatDecoder::MessageId));
    EXPECT_TRUE(Client::isSessionMessage(TestRequestDecoder::MessageId));
    EXPECT_TRUE(Client::isSessionMessage(ResendRequestDecoder::MessageId));
    EXPECT_TRUE(Client::isSessionMessage(RejectDecoder::MessageId));
    EXPECT_TRUE(Client::isSessionMessage(SequenceResetDecoder::MessageId));

    EXPECT_FALSE(Client::isSessionMessage(ExecutionReportDecoder::MessageId));
    EXPECT_FALSE(Client::isSessionMessage(NewOrderSingleDecoder::MessageId));
}

TEST(Session, WrapHeaderStampsCompileTimeIdentityAndSequence)
{
    auto client = ClientSession<FIXT_1_1, "SENDER", "TARGET">::Builder{NullStorage{}}.build();

    client.keepAlive(milliseconds{1'781'378'773'959});   // advance the session clock

    std::array<uint8_t, 256> buffer{};
    HeartbeatEncoder heartbeat{};
    client.wrapHeader(heartbeat, buffer);   // CompIDs from template, seqnum/time from session state
    const auto length = client.encode(heartbeat);
    const std::string_view encoded{reinterpret_cast<const char*>(buffer.data()), length};

    // BeginString (8) and CompIDs (49/56) come from the encoder's template
    // parameters; MsgSeqNum (34) and SendingTime (52) are stamped by wrapHeader.
    EXPECT_TRUE(encoded.starts_with("8=FIXT.1.1" SOH));
    EXPECT_NE(std::string_view::npos,
              encoded.find("49=SENDER" SOH "56=TARGET" SOH
                           "34=1" SOH "52=20260613-19:26:13.959" SOH));
}

}
