//
// Created by Fredrik Dahlberg on 2026-04-24.
//

#ifndef SIMD_FIX_MESSAGE_DECODER_HPP
#define SIMD_FIX_MESSAGE_DECODER_HPP

#include <span>
#include <expected>

#include "org/limitless/fix/decoder/Token.hpp"
#include "org/limitless/fix/decoder/FieldDecoder.hpp"
#include "org/limitless/fix/simd/LinearSearch.hpp"
#include "org/limitless/fix/utils/Utils.hpp"

namespace org::limitless::fix::decoder {

template <typename Protocol>
struct MessageDecoder
{
    FieldDecoder m_decoder[1];

    // FIXME: cache all parsed fields?
    utils::String m_sender{};           // FIXME: configuration and verification
    utils::String m_target{};           // FIXME: configuration and verification
    utils::String m_sendingTime{};
    uint32_t m_sequenceNumber{};

    MessageDecoder() = default;

    MessageDecoder(const utils::String data, const std::span<Token> tokens, const std::span<uint16_t> tags, const int32_t size)
    {
        m_decoder[0] = {data, tokens, tags, size};
    }

    void wrap(const utils::String data, const std::span<Token> tokens, const std::span<uint16_t> tags, const int32_t size)
    {
        m_decoder->wrap(data, tokens, tags, size);
    }

    [[nodiscard]] uint16_t type() const noexcept
    {
        const auto token = m_decoder->m_tokens[2];
        const auto position = token.position;
        uint16_t type = m_decoder->m_data[position];
        if (token.length == 2)
        {
            type = type + m_decoder->m_data[position + 1] * 256;
        }
        return type;
    }

    [[nodiscard]] uint16_t tokenType(const uint16_t tag) const
    {
        constexpr auto& tags = Protocol::Tags;
        const auto position = simd::find(tags.data(), tags.size(), tag);
        return position >= 0 ? Protocol::Grammar[position].type : 0;
    }

    [[nodiscard]] Result::Values checkRequired()
    {
        if (const auto sender = m_decoder->getString<49, true>())
        {
            m_sender = sender.value();
        }
        else
        {
            return Result::InvalidSenderCompId;
        }
        if (const auto target = m_decoder->getString<56, true>())
        {
            m_target = target.value();
        }
        else
        {
            return Result::InvalidTargetCompId;
        }
        if (const auto sequenceNumber = m_decoder->getUint32<34, true>())
        {
            m_sequenceNumber = sequenceNumber.value();
        }
        else
        {
            return Result::InvalidSequenceNumber;
        }
        if (const auto sendingTime = m_decoder->getString<52, true>())
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

#endif //SIMD_FIX_MESSAGE_HPP
