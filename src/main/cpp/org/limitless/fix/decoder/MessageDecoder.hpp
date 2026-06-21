//
// Created by Fredrik Dahlberg on 2026-04-24.
//

#ifndef SIMD_FIX_MESSAGE_DECODER_HPP
#define SIMD_FIX_MESSAGE_DECODER_HPP

#include <algorithm>
#include <expected>
#include <span>

#include "org/limitless/fix/CodecTypes.hpp"
#include "org/limitless/fix/decoder/FieldDecoder.hpp"

namespace org::limitless::fix::decoder {

/**
 * Base for generated message decoders (e.g. LogonDecoder, LogoutDecoder).
 * Wraps a FieldDecoder over a tokenized message and extracts the standard
 * header fields (SenderCompID, TargetCompID, MsgSeqNum, SendingTime) shared
 * by every message, validating them against an optional SessionContext.
 */
class MessageDecoder
{
private:
    const SessionContext* m_context{};

protected:
    FieldDecoder m_decoder{};

public:
    MessageDecoder() = default;

    /**
     * Constructs a decoder over an already-tokenized message.
     * @param data raw message bytes
     * @param tokens token array produced by the tokenizer
     * @param tags tag numbers, parallel to tokens
     * @param size number of valid tokens/tags
     */
    MessageDecoder(const Buffer data, const std::span<Token> tokens, const std::span<uint16_t> tags, const int32_t size)
      : m_decoder{data, tokens, tags, size}
    {
    }

    /**
     * Rebinds the decoder to a new tokenized message.
     * @param data raw message bytes
     * @param tokens token array produced by the tokenizer
     * @param tags tag numbers, parallel to tokens
     * @param size number of valid tokens/tags
     */
    void wrap(const Buffer data, const std::span<Token> tokens, const std::span<uint16_t> tags, const int32_t size)
    {
        m_decoder.wrap(data, tokens, tags, size);
    }

    /**
     * Reads the MsgType (tag 35) value from the third token. Single-byte
     * MsgTypes (e.g. 'A') are returned as-is; two-byte MsgTypes are packed
     * little-endian into the result so they remain comparable to the
     * generated MessageId constants.
     * @return MsgType value
     */
    [[nodiscard]] uint16_t type() const noexcept
    {
        const auto& token = m_decoder.tokenAt(MessageTypePosition);
        const auto position = token.m_position;
        uint16_t type = m_decoder.byteAt(position);
        if (token.m_length == 2)
        {
            type = static_cast<uint16_t>(type + (m_decoder.byteAt(position + 1) << 8));
        }
        return type;
    }

    /**
     * Sets the session context used by validate() to validate BeginString,
     * SenderCompID and TargetCompID.
     * @param context expected session parameters
     */
    void context(const SessionContext* context)
    {
        m_context = context;
    }

    /**
     * Validates SenderCompID (49), TargetCompID (56), and BeginString (8)
     * against the SessionContext, if one has been set. Field presence is
     * checked by the generated validate(); this method only compares
     * values.
     * @return Result::Success, or the first validation failure encountered
     */
    [[nodiscard]] Result::Values validateSession()
    {
        if (m_context == nullptr)
        {
            return Result::Success;
        }

        const auto beginString = m_decoder.getString<8, true, RecordType::Message>();
        if (!beginString ||
            !std::ranges::equal(beginString.value(), m_context->m_protocol))
        {
            return Result::InvalidBeginString;
        }

        const auto sender = m_decoder.getString<49, true, RecordType::Message>();
        if (!sender ||
            !std::ranges::equal(sender.value(), m_context->m_senderCompId))
        {
            return Result::InvalidSenderCompId;
        }

        const auto target = m_decoder.getString<56, true, RecordType::Message>();
        if (!target ||
            !std::ranges::equal(target.value(), m_context->m_targetCompId))
        {
            return Result::InvalidTargetCompId;
        }

        return Result::Success;
    }
};
}

#endif //SIMD_FIX_MESSAGE_DECODER_HPP
