//
// Created by Fredrik Dahlberg on 2026-04-24.
//

#ifndef SIMD_FIX_MESSAGE_DECODER_HPP
#define SIMD_FIX_MESSAGE_DECODER_HPP

#include <span>
#include <expected>

#include "org/limitless/fix/decoder/Token.hpp"
#include "org/limitless/fix/decoder/DecoderStatus.hpp"
#include "org/limitless/fix/utils/Utils.hpp"
#include "org/limitless/fix/simd/LinearSearch.hpp"

namespace org::limitless::fix::decoder {

template <typename Protocol>
struct MessageDecoder
{
    using String = std::span<const uint8_t>;

    std::span<const uint8_t> m_data{};
    std::span<Token> m_tokens{};
    std::span<uint16_t> m_tags{};
    uint32_t m_size{};

    // FIXME: cache all parsed fields?
    String m_sender{};           // FIXME: configuration and verification
    String m_target{};           // FIXME: configuration and verification
    String m_sendingTime{};
    uint32_t m_sequenceNumber{};

    MessageDecoder() = default;

    MessageDecoder(const std::span<Token> tokens, const std::span<uint16_t> tags, const uint32_t size)
        : m_tokens{tokens}, m_tags{tags}, m_size(size)
    {
    }

    MessageDecoder(const String data, const std::span<Token> tokens, const std::span<uint16_t> tags, uint32_t size)
        : m_data{data}, m_tokens{tokens}, m_tags{tags}, m_size(size)
    {
    }

    void wrap(const String data, const std::span<Token> tokens, const std::span<uint16_t> tags, const uint32_t size)
    {
        m_data = data;
        m_tokens = tokens;
        m_tags = tags;
        m_size = size;
    }

    [[nodiscard]] uint16_t type() const noexcept
    {
        const auto token = m_tokens[2];
        const auto position = token.position;
        uint16_t type = m_data[position];
        if (token.length == 2)
        {
            type = type + m_data[position + 1] * 256;
        }
        return type;
    }

    [[nodiscard]] uint16_t tokenType(const uint16_t tag) const
    {
        constexpr auto& tags = Protocol::Tags;
        const auto position = simd::find(tags.data(), tags.size(), tag);
        return position >= 0 ? Protocol::Grammar[position].type : 0;
    }

    // FIXME: tags not sorted
    [[nodiscard]] Token* next(const uint32_t tag) const
    {
        const auto index = simd::find(m_tags.data(), m_size, tag);
        return index >= 0 ? &m_tokens[index] : nullptr;
    }

    template <int32_t Tag>
    [[nodiscard]] std::expected<String, DecoderStatus> getString(const bool required) const
    {
        const auto token = next(Tag);
        if (token == nullptr)
        {
            return std::unexpected{required ? DecoderStatus::RequiredFieldMissing : DecoderStatus::NullValue};
        }
        return m_data.subspan(token->position, token->length);
    }

    template <int32_t Tag>
    [[nodiscard]] std::expected<uint32_t, DecoderStatus> getUnsigned(const bool required) const
    {
        const auto token = next(Tag);
        if (token == nullptr)
        {
            return std::unexpected{required ? DecoderStatus::RequiredFieldMissing : DecoderStatus::NullValue};
        }
        return convertToUnsigned(token);
    }

    uint32_t convertToUnsigned(const Token* token) const
    {
        return utils::asciiToDecimal(0, m_data.data() + token->position, token->length);
    }

    std::span<const uint8_t> convertToSpan(const Token* token) const
    {
        return m_data.subspan(token->position, token->length);
    }

    [[nodiscard]] DecoderStatus checkRequired()
    {
        if (const auto sender = this->getString<49>(true))
        {
            m_sender = sender.value();
        }
        else
        {
            return DecoderStatus::InvalidSenderCompId;
        }
        if (const auto target = this->getString<56>(true))
        {
            m_target = target.value();
        }
        else
        {
            return DecoderStatus::InvalidTargetCompId;
        }
        if (const auto sequenceNumber = getUnsigned<34>(true))
        {
            m_sequenceNumber = sequenceNumber.value();
        }
        else
        {
            return DecoderStatus::InvalidSequenceNumber;
        }
        if (const auto sendingTime = getString<52>(true))
        {
            m_sendingTime = sendingTime.value();
        }
        else
        {
            return DecoderStatus::InvalidSendingTime;
        }
        return DecoderStatus::Success;
    }
};
}

#endif //SIMD_FIX_MESSAGE_HPP
