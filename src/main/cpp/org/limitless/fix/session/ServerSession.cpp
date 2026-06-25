//
// Created by Fredrik Dahlberg on 2026-06-25.
//

#include "org/limitless/fix/session/ServerSession.hpp"

namespace org::limitless::fix::session
{

Result ServerSession::handle(const LogonDecoder& logon)
{
    // acceptor receives the initiator's Logon, authenticates, replies -> Active
    (void) logon;
    return Result::Success;
}

}
