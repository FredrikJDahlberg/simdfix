//
// Created by Fredrik Dahlberg on 2026-06-25.
//

#include "org/limitless/fix/session/ClientSession.hpp"

namespace org::limitless::fix::session
{

Result ClientSession::logon()
{
    // initiator sends Logon first -> SentLogon
    return Result::Success;
}

Result ClientSession::onLogon(LogonDecoder& logon)
{
    // initiator receives the acceptor's Logon acknowledgement -> Active
    (void) logon;
    return Result::Success;
}

}
