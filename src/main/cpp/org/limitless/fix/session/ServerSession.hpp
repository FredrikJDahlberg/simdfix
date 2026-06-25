//
// Created by Fredrik Dahlberg on 2026-06-25.
//

#ifndef SIMD_FIX_SERVER_SESSION_HPP
#define SIMD_FIX_SERVER_SESSION_HPP

#include "org/limitless/fix/session/Session.hpp"

namespace org::limitless::fix::session
{

/**
 * Server-side session. Waits for the initiator's Logon, authenticates it, and
 * replies with its own Logon. All other admin handling is inherited from
 * Session.
 */
class ServerSession : public Session<ServerSession>
{
public:
    /**
     * Authenticates the initiator's inbound Logon and, on success, replies with
     * a Logon and transitions the session to Active.
     * @param logon decoded inbound Logon (the initiator's request)
     * @return Success, or a failure result if authentication is rejected
     */
    Result onLogon(LogonDecoder& logon);
};

}

#endif //SIMD_FIX_SERVER_SESSION_HPP
