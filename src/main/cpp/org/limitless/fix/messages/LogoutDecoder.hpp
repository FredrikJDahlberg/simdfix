//
// Created by Fredrik Dahlberg on 2026-04-28.
//

#ifndef SIMD_FIX_LOGOUT_DECODER_HPP
#define SIMD_FIX_LOGOUT_DECODER_HPP

#include <expected>

#include "org/limitless/fix/messages/Grammar.hpp"

namespace org::limitless::fix::generated {

using namespace org::limitless::fix::parser;

struct LogoutDecoder : MessageDecoder<protocols::Logout>
{
    using Message = MessageDecoder;
    using Header = messages::HeaderDecoder<MessageDecoder>;
    using Group = messages::HopGroupDecoder<MessageDecoder>;

    static constexpr uint16_t MessageId = '5';

    Header m_header{this};
    Group m_hopGroup{this};

    LogoutDecoder() = default;

    LogoutDecoder& wrap(const std::span<const uint8_t> data, const std::span<Token> tokens, const std::span<uint16_t> tags)
    {
        Message::wrap(data, tokens, tags);
        return *this;
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

#endif //SIMD_FIX_LOGOUT_DECODER_HPP
