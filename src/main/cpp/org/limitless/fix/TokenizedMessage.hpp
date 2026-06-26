//
// Created by Fredrik Dahlberg on 2026-06-26.
//
// Public handle over a tokenized FIX message: the raw bytes plus the parser's
// field/tag spans. Part of the supported surface — it is what PayloadDecoder
// hands to a PayloadHandler and what every generated decoder wraps — so it lives
// outside detail/, though the Field/span layout it views remains internal.
//

#ifndef SIMD_FIX_TOKENIZED_MESSAGE_HPP
#define SIMD_FIX_TOKENIZED_MESSAGE_HPP

#include <cstdint>

#include "org/limitless/fix/Types.hpp"
#include "org/limitless/fix/detail/Tokens.hpp"

namespace org::limitless::fix
{

struct TokenizedMessage
{
    Buffer data;
    detail::FieldSpan fields;
    detail::TagSpan tags;
    int32_t size;

    /**
     * Reads the MsgType (tag 35) as the encoded value each generated decoder
     * exposes as its MessageId: a single byte, or two bytes packed
     * low-byte-first for two-character MsgTypes (e.g. "AB" -> 'A' | 'B' << 8).
     * @return the message id, matching <Message>Decoder::MessageId
     */
    [[nodiscard]] uint16_t messageId() const
    {
        const auto& field = fields[MessageTypePosition];
        uint16_t id = data[field.m_position];
        if (field.m_length >= 2 && data[field.m_position + 1] != FieldEnd)
        {
            id = static_cast<uint16_t>(id | (data[field.m_position + 1] << 8));
        }
        return id;
    }
};

}

#endif //SIMD_FIX_TOKENIZED_MESSAGE_HPP