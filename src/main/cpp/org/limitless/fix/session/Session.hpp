//
// Created by Fredrik Dahlberg on 2026-06-24.
//

#ifndef SIMDFIX_SESSION_HPP
#define SIMDFIX_SESSION_HPP

#include "org/limitless/fix/messages/FixMessageHandler.hpp"

namespace org::limitless::fix::session {

class Session
{
    enum class SessionState : uint8_t
    {
        Disconnected = 0, // Baseline state; no active network connection
        Connecting,       // Initiator actively opening a TCP socket link
        Connected,        // TCP socket open; awaiting or preparing the first Logon
        SentLogon,        // Client sent Logon; waiting for Server authentication
        Active,           // Session authenticated and fully synchronized (LoggedOn)
        Recovering,       // Sequence gap detected; waiting for Resend Request replay
        SentLogout,       // Logout initiated; waiting for counterparty Logout confirmation
        Closed            // Session completed cleanly or halted due to fatal error
    };

    // Optional helper function to convert states to string views for clean logging
    static constexpr std::string_view name(SessionState state) noexcept
    {
        switch (state) {
            case SessionState::Disconnected:
                return "Disconnected";
            case SessionState::Connecting:
                return "Connecting";
            case SessionState::Connected:
                return "Connected";
            case SessionState::SentLogon:
                return "SentLogon";
            case SessionState::Active:
                return "Active";
            case SessionState::Recovering:
                return "Recovering";
            case SessionState::SentLogout:
                return "SentLogout";
            case SessionState::Closed:
                return "Closed";
            default:
                return "??";
        }
    }

public:
    Result onMessage(const DecodableMessage auto& message)
    {
        Result result = Result::Success;
        switch (message.type())
        {
            case messages::LogonDecoder::MessageId:
                result = handle(static_cast<const messages::LogonDecoder&>(message));
                break;
            case messages::LogoutDecoder::MessageId:
                result = handle(static_cast<const messages::LogoutDecoder&>(message));
                break;
            case messages::HeartbeatDecoder::MessageId:
                result = handle(static_cast<const messages::HeartbeatDecoder&>(message));
                break;
            case messages::TestRequestDecoder::MessageId:
                result = handle(static_cast<const messages::TestRequestDecoder&>(message));
                break;
            case messages::ResendRequestDecoder::MessageId:
                result = handle(static_cast<const messages::ResendRequestDecoder&>(message));
                break;
            case messages::RejectDecoder::MessageId:
                result = handle(static_cast<const messages::RejectDecoder&>(message));
                break;
            case messages::SequenceResetDecoder::MessageId:
                result = handle(static_cast<const messages::SequenceResetDecoder&>(message));
                break;
            default:
                break;
        }
        return result;
    }

private:
    Result handle(const messages::LogonDecoder& logon);
    Result handle(const messages::LogoutDecoder& logout);
    Result handle(const messages::HeartbeatDecoder& heartbeat);
    Result handle(const messages::TestRequestDecoder& testRequest);
    Result handle(const messages::SequenceResetDecoder& sequenceReset);
    Result handle(const messages::RejectDecoder& reject);
    Result handle(const messages::ResendRequestDecoder& resendRequest);
};

}

#endif //SIMDFIX_SESSION_HPP
