#ifndef SIMD_FIX_MESSAGE_HANDLER_HPP
#define SIMD_FIX_MESSAGE_HANDLER_HPP

#include "org/limitless/fix/decoder/Result.hpp"
#include "org/limitless/fix/messages/FixMessageDecoders.hpp"

namespace org::limitless::fix::messages {

using decoder::Result;

template <typename Handler>
class MessageHandler
{
    LogonDecoder m_logon;
    LogoutDecoder m_logout;
    HeartbeatDecoder m_heartbeat;
    TestRequestDecoder m_testRequest;

public:
    template <typename Event>
    Result::Values receive(Event&& event)
    {
        return static_cast<Handler*>(this)->handle(std::forward<Event>(event));
    }

    void setSessionContext(const decoder::SessionContext& context)
    {
        m_logon.m_context = &context;
        m_logout.m_context = &context;
        m_heartbeat.m_context = &context;
        m_testRequest.m_context = &context;
    }

    Result::Values handle(const std::span<const uint8_t> data,
                          const std::span<Token> tokens,
                          const std::span<uint16_t> tags,
                          const uint32_t count,
                          const uint8_t messageType)
    {
        auto status = Result::InvalidMessageType;
        switch (messageType)
        {
            case LogonDecoder::MessageId:
                m_logon.wrap(data, tokens, tags, count);
                status = m_logon.checkRequired();
                if (status == Result::Success)
                {
                    status = receive(m_logon);
                }
                break;
            case LogoutDecoder::MessageId:
                m_logout.wrap(data, tokens, tags, count);
                status = m_logout.checkRequired();
                if (status == Result::Success)
                {
                    status = receive(m_logout);
                }
                break;
            case HeartbeatDecoder::MessageId:
                m_heartbeat.wrap(data, tokens, tags, count);
                status = m_heartbeat.checkRequired();
                if (status == Result::Success)
                {
                    status = receive(m_heartbeat);
                }
                break;
            case TestRequestDecoder::MessageId:
                m_testRequest.wrap(data, tokens, tags, count);
                status = m_testRequest.checkRequired();
                if (status == Result::Success)
                {
                    status = receive(m_testRequest);
                }
                break;
            default:
                break;
        }
        return status;
    }

protected:
    Result::Values handle(LogonDecoder&) { return Result::Success; }
    Result::Values handle(LogoutDecoder&) { return Result::Success; }
    Result::Values handle(HeartbeatDecoder&) { return Result::Success; }
    Result::Values handle(TestRequestDecoder&) { return Result::Success; }
};

} // namespace org::limitless::fix::messages

#endif // SIMD_FIX_MESSAGE_HANDLER_HPP
