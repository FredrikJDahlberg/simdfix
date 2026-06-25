//
// Created by Fredrik Dahlberg on 2026-06-25.
//

#ifndef SIMD_FIX_SESSION_HPP
#define SIMD_FIX_SESSION_HPP

#include <chrono>
#include <concepts>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

#include "org/limitless/fix/Types.hpp"
#include "org/limitless/fix/storage/Storage.hpp"
#include "org/limitless/fix/messages/FixMessageHandler.hpp"
#include "org/limitless/fix/messages/FixMessageEncoders.hpp"

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
 * Identity (BeginString and the two CompIDs) is a compile-time fact carried by
 * the owned encoder rather than runtime state: the session holds a
 * FixPayloadEncoder parameterized on the same Protocol/Sender/Target, which is
 * the single source of truth for tags 8, 49 and 56 on the outbound path.
 *
 * @tparam Protocol BeginString (tag 8), e.g. FIXT_1_1
 * @tparam Sender our SenderCompID (tag 49 on outbound messages)
 * @tparam Target the counterparty TargetCompID (tag 56 on outbound messages)
 * @tparam HandlerType the message handler being extended, typically
 *         MessageHandler<Role> for the concrete role session
 * @tparam Storage the message store owned by the session and used by the
 *         resend / gap-fill path; defaults to a no-op store (no persistence)
 */
template <FixedString Protocol, FixedString Sender, FixedString Target,
          FixMessageHandler HandlerType, FixStorageStrategy Storage = NullStorage>
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

    // Default heartbeat interval used when a Builder does not override it.
    static constexpr milliseconds DefaultHeartbeatPeriod{10000};

    template <typename Concrete = Session>
    class Builder;

    /**
     * Periodic timer tick: drives heartbeat emission and test-request probing
     * based on the negotiated heartbeat interval.
     * @param elapsed time since the previous tick
     * @return Success, or a failure result if the peer is unresponsive
     */
    Result keepAlive(const milliseconds now)
    {
        // if (now - m_nowMs >= m_heartbeatPeriod)
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

    [[nodiscard]] static constexpr std::string_view senderCompId() noexcept
    {
        return {Sender.Value, Sender.Size - 1};
    }

    [[nodiscard]] static constexpr std::string_view targetCompId() noexcept
    {
        return {Target.Value, Target.Size - 1};
    }

    /**
     * Binds the session's encoder to an outbound buffer and stamps the message
     * header before the body is encoded. The CompIDs (49/56) and BeginString
     * (8) come from the encoder's compile-time parameters; MsgSeqNum (34) is
     * taken from the session's outgoing sequence and SendingTime (52) from the
     * last tick. The caller then encodes the body and hands the message back to
     * the encoder's encode().
     * @tparam MessageEncoderType message encoder type, e.g. LogonEncoder
     * @param message message encoder to wrap
     * @param buffer destination buffer
     * @return message, for chaining
     */
    template <typename MessageEncoderType>
    MessageEncoderType& wrapHeader(MessageEncoderType& message, const std::span<uint8_t> buffer)
    {
        m_encoder.wrap(0, buffer);
        return m_encoder.wrapHeader(message, m_nextOutgoingSeqNum, m_nowMs);
    }

    /**
     * Finalizes an outbound message whose body has been encoded after
     * wrapHeader(): fills in MsgType and BodyLength and appends the CheckSum.
     * @tparam MessageEncoderType message encoder type, e.g. HeartbeatEncoder
     * @param message message whose body has already been encoded
     * @return total bytes written, header through trailer
     */
    template <typename MessageEncoderType>
    uint32_t encode(const MessageEncoderType& message)
    {
        return m_encoder.encode(message);
    }

    [[nodiscard]] milliseconds heartbeatPeriod() const noexcept
    {
        return m_heartbeatPeriod;
    }

protected:
    // Single source of truth for BeginString/SenderCompID/TargetCompID; baked in
    // at compile time from the Protocol/Target/Sender template parameters.
    FixPayloadEncoder<Protocol, Target, Sender> m_encoder{};
    Storage m_storage{};

    State m_state{State::Disconnected};
    uint32_t m_nextOutgoingSeqNum{1};
    uint32_t m_nextExpectedSeqNum{1};

    milliseconds m_heartbeatPeriod{DefaultHeartbeatPeriod};
    milliseconds m_nowMs{0};
};

/**
 * Fluent builder for a Session. The CompIDs are compile-time template
 * parameters of the session, so the builder takes only the message store
 * (required); the heartbeat period is optional and defaults to
 * DefaultHeartbeatPeriod.
 *
 * @tparam Concrete the session type build() returns — the base Session by
 *         default, or a concrete role session (e.g. ClientSession) that derives
 *         from it.
 */
template <FixedString Protocol, FixedString Sender, FixedString Target,
          FixMessageHandler HandlerType, FixStorageStrategy Storage>
template <typename Concrete>
class Session<Protocol, Sender, Target, HandlerType, Storage>::Builder
{
    Storage m_storage;
    milliseconds m_heartbeatPeriod{DefaultHeartbeatPeriod};

public:
    /**
     * @param storage the message store the built session takes ownership of
     */
    explicit Builder(Storage storage)
        : m_storage(std::move(storage))
    {
    }

    /**
     * Overrides the heartbeat period (default DefaultHeartbeatPeriod).
     * @param period the heartbeat interval
     * @return *this, for chaining
     */
    Builder& heartbeatPeriod(const milliseconds period)
    {
        m_heartbeatPeriod = period;
        return *this;
    }

    [[nodiscard]] Concrete build()
    {
        Concrete session;
        session.m_heartbeatPeriod = m_heartbeatPeriod;
        session.m_storage = std::move(m_storage);
        return session;
    }
};

}

#endif //SIMD_FIX_SESSION_HPP
