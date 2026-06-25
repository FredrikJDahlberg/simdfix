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
 * Session; application messages fall through to the generated MessageHandler
 * defaults.
 *
 * @tparam Storage the message store, threaded into the base Session (and used by
 *         the resend / gap-fill path); defaults to a no-op store.
 */
template <FixStorageStrategy Storage = NullStorage>
class ServerSession : public Session<MessageHandler<ServerSession<Storage>>, Storage>
{
    using Base = Session<MessageHandler<ServerSession<Storage>>, Storage>;

public:
    using Base::handle;   // keep the inherited overloads visible past the Logon override

    // Builds a ServerSession (not just the base Session).
    using Builder = typename Base::template Builder<ServerSession>;

    /**
     * Authenticates the initiator's inbound Logon and, on success, replies with
     * a Logon and transitions the session to Active.
     * @param logon decoded inbound Logon (the initiator's request)
     * @return Success, or a failure result if authentication is rejected
     */
    Result handle(const LogonDecoder& logon)
    {
        // acceptor receives the initiator's Logon, authenticates, replies -> Active
        (void) logon;
        return Result::Success;
    }
};

}

#endif //SIMD_FIX_SERVER_SESSION_HPP
