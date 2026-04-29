//
// Created by Fredrik Dahlberg on 2026-04-29.
//

#ifndef SIMD_FIX_HEADER_DECODER_HPP
#define SIMD_FIX_HEADER_DECODER_HPP

#include <expected>

//#include "org/limitless/fix/parser/MessageDecoder.hpp"
#include "Grammar.hpp"
#include "org/limitless/fix/messages/HopGroupDecoder.hpp"
#include "org/limitless/fix/parser/ParserStatus.hpp"

namespace org::limitless::fix::messages {


using namespace org::limitless::fix;

template <typename Message>
struct HeaderDecoder
{
    using Group = HopGroupDecoder<Message>;

    static constexpr uint16_t MessageId = 'A';

    const Message* m_message;

    Group m_hopGroup;

    explicit HeaderDecoder(const Message* message) : m_message(message), m_hopGroup{message}
    {
    }

    HeaderDecoder& wrap(std::span<const uint8_t> data, const std::span<Token> tokens)
    {
        Message::wrap(data, tokens);
        return *this;
    }

    std::expected<std::span<const uint8_t>, parser::ParserStatus> sender()
    {
        return m_message->getString<56>(true);
    }

    std::expected<std::span<const uint8_t>, parser::ParserStatus> target()
    {
        return m_message->getString<49>(true);
    }

    std::expected<uint32_t, parser::ParserStatus> expectedSeqNum()
    {
        return m_message->getUnsigned<34>(true);
    }

    std::expected<std::span<const uint8_t>, parser::ParserStatus> onBehalfOfCompID()
    {
        return m_message->getString<115>(false);
    }

    std::expected<uint32_t, parser::ParserStatus> nextExpectedSeqNum()
    {
        return m_message->getUnsigned<789>(false);
    }

    Group& hopGroup()
    {
        return m_hopGroup.wrap();
    }
};
}

#endif //SIMD_FIX_HEADER_DECODER_HPP
