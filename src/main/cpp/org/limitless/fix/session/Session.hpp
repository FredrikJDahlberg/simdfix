//
// Created by Fredrik Dahlberg on 2026-06-25.
//

#ifndef SIMD_FIX_SESSION_HPP
#define SIMD_FIX_SESSION_HPP

#include <chrono>
#include <concepts>
#include <cstdint>
#include <string_view>

#include "org/limitless/fix/messages/FixMessageHandler.hpp"

namespace org::limitless::fix::session
{

using namespace std::chrono;
using namespace org::limitless::fix::messages;

/**
 * Describes the message-handler interface the decoder drives: a type the
 * PayloadDecoder can dispatch a tokenized message to (see
 * PayloadDecoder::parse, which calls handler.handle(message, messageType)) and
 * bind a SessionContext to. Both the generated MessageHandler and the session
 * engine layered on top of it model this concept.
 */
template <typename MessageHandler>
concept FixMessageHandler = requires(MessageHandler handler,
                                     const TokenizedMessage& message,
                                     const uint16_t messageType,
                                     const SessionContext& context)
{
    { handler.handle(message, messageType) } -> std::same_as<Result>;
    { handler.setSessionContext(context) };
};

/**
 * Role-agnostic half of the FIX session layer: the state machine, sequence-number
 * bookkeeping, and the admin-message handling (Heartbeat, TestRequest, Logout,
 * Reject, ResendRequest, SequenceReset) that is identical for initiator and
 * acceptor.
 *
 * Layered onto a message handler: Session derives from the handler the decoder
 * dispatches to, so it is itself a FixMessageHandler. It overloads the admin
 * messages it owns and leaves application messages (and the one role-specific
 * message, Logon) to the handler below and the concrete role above.
 *
 * @tparam Handler the message handler being extended, typically
 *         MessageHandler<Role> for the concrete role session
 */
template <FixMessageHandler Handler>
class Session : public Handler
{
    enum class State : uint8_t
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
    [[nodiscard]] constexpr std::string_view name(const State state) noexcept
    {
        switch (state)
        {
            case State::Disconnected: return "Disconnected";
            case State::Connecting: return "Connecting";
            case State::Connected: return "Connected";
            case State::SentLogon: return "SentLogon";
            case State::Active: return "Active";
            case State::Recovering: return "Recovering";
            case State::SentLogout: return "SentLogout";
            case State::Closed: return "Closed";
            case State::Null: default: return "??";
        }
    }

public:
    // Keep the two-argument dispatcher, the application-message defaults
    // (ExecutionReport, NewOrderSingle), and the Logon default from the handler
    // below visible alongside the admin-message overloads declared here.
    using Handler::handle;

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

    [[nodiscard]] State state() const noexcept
    {
        return m_state;
    }

protected:
    State m_state{State::Disconnected};
    uint32_t m_nextOutgoingSeqNum{1};
    uint32_t m_nextExpectedSeqNum{1};

    milliseconds m_now{0};
    milliseconds m_heartBtInt{};
};

}

#endif //SIMD_FIX_SESSION_HPP
