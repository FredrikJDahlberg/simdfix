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

    static constexpr uint16_t MessageId = '5';

    LogoutDecoder& wrap(std::span<const uint8_t> data, const std::span<Token> tokens)
    {
        Message::wrap(data,tokens);
        return *this;
    }
};

}

#endif //SIMD_FIX_LOGOUT_DECODER_HPP
