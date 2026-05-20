//
// Created by Fredrik Dahlberg on 2026-04-29.
//

#ifndef SIMD_FIX_HEADER_DECODER_HPP
#define SIMD_FIX_HEADER_DECODER_HPP

#include <expected>

#include "org/limitless/fix/messages/HopGroupDecoder.hpp"
#include "org/limitless/fix/decoder/DecoderStatus.hpp"

namespace org::limitless::fix::messages {

using namespace org::limitless::fix;

template <typename Message>
struct HeaderDecoder
{
    using Group = HopGroupDecoder<Message>;
    using String = std::span<const uint8_t>;

    static constexpr uint16_t MessageId = 'A';

    const Message* m_message;

    String m_sender;
    Group m_hopGroup;

    explicit HeaderDecoder(const Message* message) : m_message(message), m_hopGroup{message}
    {
    }

    HeaderDecoder& wrap(String data, const std::span<Token> tokens)
    {
        Message::wrap(data, tokens);
        return *this;
    }

    [[nodiscard]] std::expected<String, decoder::DecoderStatus> sender()
    {
        return m_message->m_sender;
    }

    [[nodiscard]] std::expected<String, decoder::DecoderStatus> target()
    {
        return m_message->m_target;
    }

    [[nodiscard]] std::expected<uint32_t, decoder::DecoderStatus> sequenceNumber()
    {
        return m_message->m_sequenceNumber;
    }

    [[nodiscard]] std::expected<String, decoder::DecoderStatus> sendingTime()
    {
        return m_message->m_sendingTime;
    }

    [[nodiscard]] std::expected<String, decoder::DecoderStatus> onBehalfOfCompID()
    {
        return m_message->template getString<115>(false);
    }

    [[nodiscard]] std::expected<uint32_t, decoder::DecoderStatus> nextExpectedSeqNum() const
    {
        return m_message->template getUnsigned<789>(false);
    }

    Group& hopGroup()
    {
        return m_hopGroup.wrap();
    }
};
}

#endif //SIMD_FIX_HEADER_DECODER_HPP
