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

#include "org/limitless/simdfix/Types.hpp"
#include "org/limitless/simdfix/detail/Tokens.hpp"
#include "org/limitless/simdfix/detail/parser/FieldDecoder.hpp"

namespace org::limitless::simdfix
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

    /**
     * Looks up Tag outside any repeating group and parses its value as an
     * unsigned 32-bit integer. For reading a header field before the message
     * type is known and a generated decoder has been chosen; once one has,
     * read through the decoder instead.
     * @tparam Tag tag number to read
     * @return field value, or Result::Success if the tag is absent
     */
    template <int32_t Tag>
    [[nodiscard]] Uint32Result getUint32() const
    {
        return decoder().getUint32<Tag, false, detail::RecordType::Message>();
    }

    /**
     * Looks up Tag outside any repeating group and returns its value as a
     * string view into the message buffer. Same purpose as getUint32.
     * @tparam Tag tag number to read
     * @return field value, or Result::Success if the tag is absent
     */
    template <int32_t Tag>
    [[nodiscard]] StringResult getString() const
    {
        return decoder().getString<Tag, false, detail::RecordType::Message>();
    }

private:
    [[nodiscard]] detail::parser::FieldDecoder decoder() const
    {
        return {data, fields, tags, size};
    }
};

}

#endif //SIMD_FIX_TOKENIZED_MESSAGE_HPP