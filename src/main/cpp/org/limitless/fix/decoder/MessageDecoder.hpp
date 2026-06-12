//
// Created by Fredrik Dahlberg on 2026-04-24.
//

#ifndef SIMD_FIX_MESSAGE_DECODER_HPP
#define SIMD_FIX_MESSAGE_DECODER_HPP

#include <algorithm>
#include <chrono>
#include <span>
#include <expected>
#include <utility>

#include "org/limitless/fix/DecoderTypes.hpp"
#include "org/limitless/fix/decoder/FieldDecoder.hpp"

namespace org::limitless::fix::decoder {

/**
 * Base for generated message decoders (e.g. LogonDecoder, LogoutDecoder).
 * Wraps a FieldDecoder over a tokenized message and extracts the standard
 * header fields (SenderCompID, TargetCompID, MsgSeqNum, SendingTime) shared
 * by every message, validating them against an optional SessionContext.
 */
struct MessageDecoder
{
    FieldDecoder m_decoder{};

    String m_sender{};
    String m_target{};
    std::chrono::milliseconds m_sendingTime{};
    uint32_t m_sequenceNumber{};
    const SessionContext* m_context{};

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
        const auto token = m_decoder.tokenAt(2);
        const auto position = token.m_position;
        uint16_t type = m_decoder.byteAt(position);
        if (token.m_length == 2)
        {
            type = type + m_decoder.byteAt(position + 1) * 256;
        }
        return type;
    }

    /**
     * Extracts and validates the standard header fields: SenderCompID (49),
     * TargetCompID (56), MsgSeqNum (34), and SendingTime (52). If m_context
     * is set and specifies expected CompIDs, the corresponding field must
     * match or the message is rejected.
     * @return Result::Success, or the first validation failure encountered
     */
    [[nodiscard]] Result::Values checkRequired()
    {
        if (const auto sender = m_decoder.getString<49, true, ParentType::Component>())
        {
            m_sender = sender.value();
            if (m_context)

                if (m_context != nullptr && !m_context->m_expectedSenderCompId.empty()
                    && !std::ranges::equal(std::as_const(m_sender),
                                           std::as_const(m_context->m_expectedSenderCompId)))
                {
                    return Result::InvalidSenderCompId;
                }

            if (m_context)

                if (m_context != nullptr && !m_context->m_expectedSenderCompId.empty()
                    && !std::ranges::equal(m_sender, m_context->m_expectedSenderCompId))
                {
                    return Result::InvalidSenderCompId;
                }
        }
        else
        {
            return Result::InvalidSenderCompId;
        }
        if (const auto target = m_decoder.getString<56, true, ParentType::Component>())
        {
            m_target = target.value();
            if (m_context != nullptr && !m_context->m_expectedTargetCompId.empty()
                && !std::ranges::equal(m_target, m_context->m_expectedTargetCompId))
            {
                return Result::InvalidTargetCompId;
            }
        }
        else
        {
            return Result::InvalidTargetCompId;
        }
        if (const auto sequenceNumber = m_decoder.getUint32<34, true, ParentType::Component>())
        {
            m_sequenceNumber = sequenceNumber.value();
        }
        else
        {
            return Result::InvalidSequenceNumber;
        }
        if (const auto sendingTime = m_decoder.getTimestamp<52, true, ParentType::Component>())
        {
            m_sendingTime = sendingTime.value();
        }
        else
        {
            return Result::InvalidSendingTime;
        }
        return Result::Success;
    }
};
}

#endif //SIMD_FIX_MESSAGE_DECODER_HPP
