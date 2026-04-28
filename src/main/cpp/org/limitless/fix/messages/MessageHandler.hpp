//
// Created by Fredrik Dahlberg on 2026-04-25.
//

#ifndef SIMD_FIX_MESSAGE_HANDLER_HPP
#define SIMD_FIX_MESSAGE_HANDLER_HPP

#include "org/limitless/fix/parser/ParserStatus.hpp"
#include "org/limitless/fix/messages/LogonDecoder.hpp"
#include "org/limitless/fix/messages/LogoutDecoder.hpp"

namespace org::limitless::fix::generated {

template <typename Handler>
class MessageHandler
{
    LogonDecoder m_logon{};
    LogoutDecoder m_logout{};

public:
    template <typename Event>
    ParserStatus receive(Event&& event)
    {
        return static_cast<Handler*>(this)->handle(std::forward<Event>(event));
    }

    ParserStatus handle(const std::span<const uint8_t> data, const std::span<Token> tokens)
    {
        const auto messageType = data[tokens[2].position]; // FIXME
        ParserStatus status = ParserStatus::InvalidMessageType;
        switch (messageType)
        {
            case LogonDecoder::MessageId:
                m_logon.wrap(data, tokens);
                status = m_logon.checkRequired();
                if (status == ParserStatus::Success)
                {
                    status = receive(m_logon);
                }
                break;
            case LogoutDecoder::MessageId:
                m_logout.wrap(data, tokens);
                status = m_logout.checkRequired();
                if (status == ParserStatus::Success)
                {
                    status = receive(m_logout);
                }
                break;

            default:
                break;
        }
        return status;
    }

protected:
    ParserStatus handle(LogonDecoder& message) { return ParserStatus::Success; }
    ParserStatus handle(LogoutDecoder& message) { return ParserStatus::Success; }
};

}

#endif // SIMD_FIX_MESSAGE_HANDLER_HPP
