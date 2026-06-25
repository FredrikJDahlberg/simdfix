//
// Created by Fredrik Dahlberg on 2026-06-25.
//

#include <gtest/gtest.h>

#include <type_traits>

#include "org/limitless/fix/session/ClientSession.hpp"
#include "org/limitless/fix/session/ServerSession.hpp"
#include "org/limitless/fix/storage/MemoryStorage.hpp"

namespace org::limitless::fix::session
{

using namespace std::chrono;

TEST(Session, BuilderProducesConcreteRole)
{
    auto client = ClientSession<>::Builder{"CLIENT", "EXCH", NullStorage{}}.build();
    static_assert(std::is_same_v<decltype(client), ClientSession<>>);
    EXPECT_EQ("CLIENT", client.senderCompId());
    EXPECT_EQ("EXCH", client.targetCompId());
    EXPECT_EQ(milliseconds{10000}, client.heartbeatPeriod());   // default constant

    auto server = ServerSession<>::Builder{"EXCH", "CLIENT", NullStorage{}}
                      .heartbeatPeriod(milliseconds{20000})
                      .build();
    static_assert(std::is_same_v<decltype(server), ServerSession<>>);
    EXPECT_EQ("EXCH", server.senderCompId());
    EXPECT_EQ(milliseconds{20000}, server.heartbeatPeriod());
}

TEST(Session, MemoryStorageBackedRole)
{
    using Client = ClientSession<storage::MemoryStorage>;

    auto client = Client::Builder{"CLIENT", "EXCH", storage::MemoryStorage{}}
                      .heartbeatPeriod(milliseconds{30000})
                      .build();
    static_assert(std::is_same_v<decltype(client), Client>);
    EXPECT_EQ("CLIENT", client.senderCompId());
    EXPECT_EQ(milliseconds{30000}, client.heartbeatPeriod());

    using Server = ServerSession<storage::MemoryStorage>;
    auto server = Server::Builder{"EXCH", "CLIENT", storage::MemoryStorage{}}.build();
    static_assert(std::is_same_v<decltype(server), Server>);
    EXPECT_EQ("EXCH", server.senderCompId());
}

TEST(Session, IsSessionMessageClassifiesAdminMessages)
{
    using Client = ClientSession<>;
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

}
