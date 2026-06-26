//
// Created by Fredrik Dahlberg on 2026-04-24.
//

#ifndef SIMD_FIX_MESSAGE_DECODER_HPP
#define SIMD_FIX_MESSAGE_DECODER_HPP

#include <algorithm>
#include "org/limitless/fix/detail/Expected.hpp"
#include <span>

#include "org/limitless/fix/detail/Tokens.hpp"
#include "../detail/decoder/FieldDecoder.hpp"

namespace org::limitless::fix::decoder {

using namespace org::limitless::fix::detail;
using namespace org::limitless::fix::detail::decoder;

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

    MessageDecoder(const SessionContext* context, const TokenizedMessage& message) : m_context{context}
    {
        m_decoder.wrap(message.data, message.fields, message.tags, message.size);
    }

    /**
     * Reads the MsgType (tag 35) value from the third field. Single-byte
     * MsgTypes (e.g. 'A') are returned as-is; two-byte MsgTypes are packed
     * little-endian into the result so they remain comparable to the
     * generated MessageId constants.
     * @return MsgType value
     */
    [[nodiscard]] uint16_t type() const noexcept
    {
        const auto& field = m_decoder.fieldAt(MessageTypePosition);
        const auto position = field.m_position;
        uint16_t type = m_decoder.byteAt(position);
        if (field.m_length >= 2 && m_decoder.byteAt(position + 1) != FieldEnd)
        {
            type = static_cast<uint16_t>(type | (m_decoder.byteAt(position + 1) << 8));
        }
        return type;
    }

    /**
     * Validates SenderCompID (49), TargetCompID (56), and BeginString (8)
     * against the SessionContext, if one has been set. Field presence is
     * checked by the generated validate(); this method only compares
     * values.
     * @return Result::Success, or the first validation failure encountered
     */
    [[nodiscard]] Result validateSession()
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
