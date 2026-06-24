//
// Created by Fredrik Dahlberg on 2026-06-24.
//

#include "org/limitless/fix/session/Session.hpp"

namespace org::limitless::fix::session {

Result Session::handle(const messages::LogonDecoder& logon)
{
    // client send, server receive
    return Result::Success;
}

Result Session::handle(const messages::LogoutDecoder& logout)
{
    // clent send/receive, server send/receive
    return Result::Success;
}

Result Session::handle(const messages::HeartbeatDecoder& heartbeat)
{
    // clent send/receive, server send/receive
    return Result::Success;
}

Result Session::handle(const messages::TestRequestDecoder& test)
{
    // clent send/receive, server send/receive
    return Result::Success;
}

Result Session::handle(const messages::SequenceResetDecoder& sequenceReset)
{
    // client send/gapfill, server send/receive
    return Result::Success;
}

Result Session::handle(const messages::RejectDecoder& reject)
{
    // client send/receive, server send/receive
    return Result::Success;
}

Result Session::handle(const messages::ResendRequestDecoder& resendRequest)
{
    // clent send/receive, server send/receive
    return Result::Success;
}

}
