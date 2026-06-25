//
// Created by Fredrik Dahlberg on 2026-06-25.
//

#ifndef SIMD_FIX_SESSION_HPP
#define SIMD_FIX_SESSION_HPP

#include <chrono>
#include <concepts>
#include <cstdint>
#include <string_view>

#include "org/limitless/fix/storage/Storage.hpp"
#include "org/limitless/fix/messages/FixMessageHandler.hpp"

namespace org::limitless::fix::session
{

using namespace std::chrono;
using namespace org::limitless::fix::messages;
using namespace org::limitless::fix::storage;

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
 * @tparam HandlerType the message handler being extended, typically
 *         MessageHandler<Role> for the concrete role session
 * @tparam Storage the message store owned by the session and used by the
 *         resend / gap-fill path; defaults to a no-op store (no persistence)
 */
template <FixMessageHandler HandlerType, FixStorageStrategy Storage = NullStorage>
class Session : public HandlerType
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
    using HandlerType::handle;

    class Builder;

    /**
     * Periodic timer tick: drives heartbeat emission and test-request probing
     * based on the negotiated heartbeat interval.
     * @param elapsed time since the previous tick
     * @return Success, or a failure result if the peer is unresponsive
     */
    Result keepAlive(const milliseconds now)
    {
        // if (now - m_nowMs >= HeartbeatPeriodMillis)
        {

        }
        m_nowMs = now;
        return Result::Success;
    }

    Result handle(const LogoutDecoder& logout)
    {
        keepAlive(logout.sendingTime());

        return Result::Success;
    }

    Result handle(const HeartbeatDecoder& heartbeat)
    {
        keepAlive(heartbeat.sendingTime());
        return Result::Success;
    }

    Result handle(const TestRequestDecoder& testRequest)
    {
        keepAlive(testRequest.sendingTime());
        return Result::Success;
    }

    Result handle(const ResendRequestDecoder& resendRequest)
    {
        keepAlive(resendRequest.sendingTime());
        return Result::Success;
    }

    Result handle(const RejectDecoder& reject)
    {
        keepAlive(reject.sendingTime());
        return Result::Success;
    }

    Result handle(const SequenceResetDecoder& sequenceReset)
    {
        keepAlive(sequenceReset.sendingTime());
        return Result::Success;
    }

    /**
     * Classifies a MsgType as a session (administrative) message — one handled
     * by the session layer itself — versus an application message left to the
     * handler below.
     * @param messageId the MsgType value (as produced by the decoder)
     * @return true for Logon, Logout, Heartbeat, TestRequest, ResendRequest,
     *         Reject and SequenceReset; false otherwise
     */
    [[nodiscard]] static constexpr bool isSessionMessage(const uint16_t messageId) noexcept
    {
        return messageId == HeartbeatDecoder::MessageId || messageId == TestRequestDecoder::MessageId ||
               messageId == ResendRequestDecoder::MessageId || messageId == RejectDecoder::MessageId ||
               messageId == LogonDecoder::MessageId || messageId == LogoutDecoder::MessageId ||
               messageId == SequenceResetDecoder::MessageId;
    }

    [[nodiscard]] State state() const noexcept
    {
        return m_state;
    }

protected:
    Storage m_storage{};

    State m_state{State::Disconnected};
    uint32_t m_nextOutgoingSeqNum{1};
    uint32_t m_nextExpectedSeqNum{1};

    milliseconds m_nowMs{0};
    //static constexpr milliseconds HeartbeatPeriodMs;
};

template <FixMessageHandler HandlerType, FixStorageStrategy Storage>
class Session<HandlerType, Storage>::Builder
{

};



}

#endif //SIMD_FIX_SESSION_HPP
