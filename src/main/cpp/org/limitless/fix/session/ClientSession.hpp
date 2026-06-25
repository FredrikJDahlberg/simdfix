//
// Created by Fredrik Dahlberg on 2026-06-25.
//

#ifndef SIMD_FIX_CLIENT_SESSION_HPP
#define SIMD_FIX_CLIENT_SESSION_HPP

#include "org/limitless/fix/session/Session.hpp"

namespace org::limitless::fix::session
{

/**
 * Client-side session. Drives the connection and the Logon handshake: it sends
 * the initial Logon, then validates the acceptor's Logon response. All other
 * admin handling is inherited from Session.
 */
class ClientSession : public Session<ClientSession>
{
public:
    /**
     * Begins the handshake by emitting the initial Logon and moving to
     * SentLogon.
     * @return Success once the Logon has been sent
     */
    Result logon();

    /**
     * Validates the acceptor's Logon response and, on success, transitions the
     * session to Active.
     * @param logon decoded inbound Logon (the acceptor's acknowledgement)
     * @return Success, or a failure result if the response is rejected
     */
    Result onLogon(LogonDecoder& logon);
};

}

#endif //SIMD_FIX_CLIENT_SESSION_HPP
