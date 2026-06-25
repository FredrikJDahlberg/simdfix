//
// Created by Fredrik Dahlberg on 2026-06-25.
//

#ifndef SIMD_FIX_SESSION_HPP
#define SIMD_FIX_SESSION_HPP

#include <chrono>
#include <cstdint>
#include <string_view>

#include "org/limitless/fix/messages/FixMessageHandler.hpp"

namespace org::limitless::fix::session
{

using namespace std::chrono;
using namespace org::limitless::fix::messages;

// Lifecycle states shared by both session roles.
enum class SessionState : uint8_t
{
    Null = 0,
    Disconnected,     // Baseline state; no active network connection
    Connecting,       // Initiator actively opening a TCP socket link
    Connected,        // TCP socket open; awaiting or preparing the first Logon
    SentLogon,        // Client sent Logon; waiting for Server authentication
    Active,           // Session authenticated and fully synchronized (LoggedOn)
    Recovering,       // Sequence gap detected; waiting for Resend Request replay
    SentLogout,       // Logout initiated; waiting for counterparty Logout confirmation
    Closed            // Session completed cleanly or halted due to fatal error
};

// Optional helper function to convert states to string views for clean logging
[[nodiscard]] constexpr std::string_view name(const SessionState state) noexcept
{
    switch (state)
    {
        case SessionState::Disconnected: return "Disconnected";
        case SessionState::Connecting: return "Connecting";
        case SessionState::Connected: return "Connected";
        case SessionState::SentLogon: return "SentLogon";
        case SessionState::Active: return "Active";
        case SessionState::Recovering: return "Recovering";
        case SessionState::SentLogout: return "SentLogout";
        case SessionState::Closed: return "Closed";
        case SessionState::Null: default: return "??";
    }
}

/**
 * Role-agnostic half of the FIX session layer: the state machine, sequence-number
 * bookkeeping, and the admin-message handling (Heartbeat, TestRequest, Logout,
 * Reject, ResendRequest, SequenceReset) that is identical for initiator and
 * acceptor.
 *
 * Inherits the generated MessageHandler<Derived> dispatcher, so a decoded
 * TokenizedMessage routes straight to the handle() overloads below. The only
 * role-specific fork — Logon — is delegated to Derived::onLogon via CRTP, so the
 * concrete sessions implement just that hook plus any connection setup.
 *
 * @tparam Derived the concrete role session (ClientSession / ServerSession)
 */
template <typename Derived>
class Session : public MessageHandler<Derived>
{
public:
    // Keep the two-argument dispatcher and the application-message defaults
    // (ExecutionReport, NewOrderSingle) from the base visible alongside the
    // admin-message overloads declared here.
    using MessageHandler<Derived>::handle;

    /**
     * Periodic timer tick: drives heartbeat emission and test-request probing
     * based on the negotiated heartbeat interval.
     * @param elapsed time since the previous tick
     * @return Success, or a failure result if the peer is unresponsive
     */
    Result heartbeat(const milliseconds elapsed)
    {
        (void) elapsed;
        return Result::Success;
    }

    /**
     * Logon is the one admin message whose handling differs by role, so it is
     * forwarded to the concrete session.
     * @param logon decoded inbound Logon
     * @return result of the role-specific handling
     */
    Result handle(LogonDecoder& logon)
    {
        return static_cast<Derived*>(this)->onLogon(logon);
    }

    Result handle(LogoutDecoder& logout)
    {
        (void) logout;
        return Result::Success;
    }

    Result handle(HeartbeatDecoder& heartbeat)
    {
        (void) heartbeat;
        return Result::Success;
    }

    Result handle(TestRequestDecoder& testRequest)
    {
        (void) testRequest;
        return Result::Success;
    }

    Result handle(ResendRequestDecoder& resendRequest)
    {
        (void) resendRequest;
        return Result::Success;
    }

    Result handle(RejectDecoder& reject)
    {
        (void) reject;
        return Result::Success;
    }

    Result handle(SequenceResetDecoder& sequenceReset)
    {
        (void) sequenceReset;
        return Result::Success;
    }

    [[nodiscard]] SessionState state() const noexcept
    {
        return m_state;
    }

protected:
    SessionState m_state{SessionState::Disconnected};
    uint32_t m_nextOutgoingSeqNum{1};
    uint32_t m_nextExpectedSeqNum{1};

    milliseconds m_timestamp{0};
    milliseconds m_heartBtInt{};
};

}

#endif //SIMD_FIX_SESSION_HPP
