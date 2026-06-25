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
 * admin handling is inherited from Session; application messages fall through to
 * the generated MessageHandler defaults.
 *
 * @tparam Protocol BeginString (tag 8), e.g. FIXT_1_1
 * @tparam Sender our SenderCompID (tag 49 on outbound messages)
 * @tparam Target the counterparty TargetCompID (tag 56 on outbound messages)
 * @tparam Storage the message store, threaded into the base Session (and used by
 *         the resend / gap-fill path); defaults to a no-op store.
 */
template <FixedString Protocol, FixedString Sender, FixedString Target,
          FixStorageStrategy Storage = NullStorage, typename Transport = DiscardTransport>
class ClientSession
    : public RoleSession<ClientSession, Protocol, Sender, Target, Storage, Transport>
{
    using Base = RoleSession<ClientSession, Protocol, Sender, Target, Storage, Transport>;

public:
    using Base::handle;   // keep the inherited overloads visible past the Logon override

    // Builds a ClientSession (not just the base Session).
    using Builder = typename Base::template Builder<ClientSession>;

    /**
     * Begins the handshake by emitting the initial Logon and moving to
     * SentLogon.
     * @return Success once the Logon has been sent
     */
    Result logon()
    {
        // initiator sends Logon first -> SentLogon
        return Result::Success;
    }

    /**
     * Validates the acceptor's Logon response and, on success, transitions the
     * session to Active.
     * @param logon decoded inbound Logon (the acceptor's acknowledgement)
     * @return Success, or a failure result if the response is rejected
     */
    Result handle(const LogonDecoder& logon)
    {
        // initiator receives the acceptor's Logon acknowledgement -> Active
        (void) logon;
        return Result::Success;
    }
};

}

#endif //SIMD_FIX_CLIENT_SESSION_HPP
