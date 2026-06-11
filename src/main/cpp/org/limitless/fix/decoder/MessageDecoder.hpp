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
#include <ranges>

#include "org/limitless/fix/decoder/FieldDecoder.hpp"
#include "org/limitless/fix/decoder/DecoderTypes.hpp"
#include "org/limitless/fix/utils/Utils.hpp"

namespace org::limitless::fix::decoder {

struct MessageDecoder
{
    FieldDecoder m_decoder{};

    utils::String m_sender{};
    utils::String m_target{};
    std::chrono::milliseconds m_sendingTime{};
    uint32_t m_sequenceNumber{};
    const SessionContext* m_context{};

    MessageDecoder() = default;

    MessageDecoder(const utils::Buffer data, const std::span<Token> tokens, const std::span<uint16_t> tags, const int32_t size)
      : m_decoder{data, tokens, tags, size}
    {
    }

    void wrap(const utils::Buffer data, const std::span<Token> tokens, const std::span<uint16_t> tags, const int32_t size)
    {
        m_decoder.wrap(data, tokens, tags, size);
    }

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
