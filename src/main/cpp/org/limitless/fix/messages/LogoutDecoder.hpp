//
// Created by Fredrik Dahlberg on 2026-04-28.
//

#ifndef SIMD_FIX_LOGOUT_DECODER_HPP
#define SIMD_FIX_LOGOUT_DECODER_HPP

#include <expected>

namespace org::limitless::fix::generated {

using namespace org::limitless::fix::parser;

struct LogoutGrammar
{
    static constexpr std::array<Entry<Dictionary>, 9> Meta {
        {
            {1, {1, 0, false} },
            {10, {10, 0, true}},
            {34, {34, 0, true}},
            {49, {49, 12, true}},
            {102, {102, 24, true}},
            {627, {627, 10, false}},
            {628, {628, 10, false}},
            {629, {629, 10, false}},
            {630, {630, 10, false}}
        }};
};

struct LogoutDecoder : MessageDecoder<LogoutGrammar>
{
    static constexpr uint16_t MessageId = '5';

    LogoutDecoder& wrap(std::span<const uint8_t> data, const std::span<Token> tokens)
    {
        Message::wrap(data,tokens);
        return *this;
    }
};

}

#endif //SIMD_FIX_LOGOUTDECODER_HPP
