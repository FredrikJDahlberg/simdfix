//
// Created by Fredrik Dahlberg on 2026-04-25.
//
// Will be generated
//

#ifndef SIMD_FIX_LOGON_DECODER_HPP
#define SIMD_FIX_LOGON_DECODER_HPP

#include <expected>

#include "HeaderDecoder.hpp"
#include "org/limitless/fix/parser/MessageDecoder.hpp"
#include "org/limitless/fix/parser/ParserStatus.hpp"
#include "org/limitless/fix/messages/Grammar.hpp"

namespace org::limitless::fix::generated {

using namespace org::limitless::fix;

struct LogonDecoder : parser::MessageDecoder<protocols::Logon>
{
    using Message = MessageDecoder;
    using Header = messages::HeaderDecoder<MessageDecoder>;
    using Group = messages::HopGroupDecoder<MessageDecoder>;

    static constexpr uint16_t MessageId = 'A';

    Header m_header{this};
    Group m_hopGroup{this};

    LogonDecoder() = default;

    LogonDecoder& wrap(std::span<const uint8_t> data, const std::span<Token> tokens)
    {
        Message::wrap(data, tokens);
        return *this;
    }

    std::expected<std::span<const uint8_t>, parser::ParserStatus> sender()
    {
        return this->getString<56>(true);
    }

    std::expected<std::span<const uint8_t>, parser::ParserStatus> target()
    {
        return this->getString<49>(true);
    }

    std::expected<uint32_t, parser::ParserStatus> expectedSeqNum()
    {
        return this->getUnsigned<34>(true);
    }

    std::expected<std::span<const uint8_t>, parser::ParserStatus> onBehalfOfCompID()
    {
        return this->getString<115>(false);
    }

    std::expected<uint32_t, parser::ParserStatus> nextExpectedSeqNum()
    {
        return this->getUnsigned<789>(false);
    }

    Header& header()
    {
        return m_header;
    }

    Group& hopGroup()
    {
        return m_hopGroup;
    }

};
}

#endif //SIMD_FIX_LOGON_DECODER_HPP
