#ifndef SIMD_FIX_MESSAGE_HANDLER_HPP
#define SIMD_FIX_MESSAGE_HANDLER_HPP

#include "org/limitless/fix/decoder/DecoderStatus.hpp"
#include "org/limitless/fix/messages/MessageDecoders.hpp"

namespace org::limitless::fix::generated {

using decoder::DecoderStatus;

template <typename Handler>
class MessageHandler
{
    LogonDecoder m_logon;
    LogoutDecoder m_logout;
    HeartbeatDecoder m_heartbeat;

public:
    template <typename Event>
    DecoderStatus receive(Event&& event)
    {
        return static_cast<Handler*>(this)->handle(std::forward<Event>(event));
    }

    DecoderStatus handle(const std::span<const uint8_t> data,
                        const std::span<Token> tokens,
                        const std::span<uint16_t> tags,
                        const uint32_t count)
    {
        const auto messageType = data[tokens[2].position];
        auto status = DecoderStatus::InvalidMessageType;
        switch (messageType)
        {
            case LogonDecoder::MessageId:
                m_logon.wrap(data, tokens, tags, count);
                status = m_logon.checkRequired();
                if (status == DecoderStatus::Success)
                {
                    status = receive(m_logon);
                }
                break;
            case LogoutDecoder::MessageId:
                m_logout.wrap(data, tokens, tags, count);
                status = m_logout.checkRequired();
                if (status == DecoderStatus::Success)
                {
                    status = receive(m_logout);
                }
                break;
            case HeartbeatDecoder::MessageId:
                m_heartbeat.wrap(data, tokens, tags, count);
                status = m_heartbeat.checkRequired();
                if (status == DecoderStatus::Success)
                {
                    status = receive(m_heartbeat);
                }
                break;
            default:
                break;
        }
        return status;
    }

protected:
    DecoderStatus handle(LogonDecoder&) { return DecoderStatus::Success; }
    DecoderStatus handle(LogoutDecoder&) { return DecoderStatus::Success; }
    DecoderStatus handle(HeartbeatDecoder&) { return DecoderStatus::Success; }
};

} // namespace org::limitless::fix::generated

#endif // SIMD_FIX_MESSAGE_HANDLER_HPP
