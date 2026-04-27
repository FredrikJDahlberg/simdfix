//
// Created by Fredrik Dahlberg on 2026-04-24.
//

#include <span>

#include "org/limitless/fix/parser/Dictionary.hpp"
#include "org/limitless/fix/parser/ParserStatus.hpp"
#include "org/limitless/fix/parser/Tokenizer.hpp"
#include "org/limitless/fix/parser/Utils.hpp"
#include "org/limitless/fix/parser/BitSet64.hpp"

#ifndef SIMD_FIX_MESSAGE_DECODER_HPP
#define SIMD_FIX_MESSAGE_DECODER_HPP

namespace org::limitless::fix::parser {

struct MessageDecoder
{
    using Token = Tokenizer::Token;

    std::span<const uint8_t> m_data{};
    std::span<Token> m_tokens{};
    BitSet64 m_present{};

    // FIXME: nested group stack
    static constexpr std::span<uint8_t> NullSpan{};

    MessageDecoder() = default;
#if 0
    MessageDecoder(const std::span<const uint8_t> data, const std::span<Token> tokens, const BitSet64 present)
        : m_data{data}, m_tokens{tokens}, m_present{present}
    {
    }
#endif
    void wrap(const std::span<const uint8_t> data, const std::span<Token> tokens, const BitSet64 present)
    {
        m_data = data;
        m_tokens = tokens;
        m_present = present;
    }
#if 0
    [[nodiscard]] uint8_t messageType() const
    {
        auto messageType = m_tokens[2];
        return m_data[messageType.position];
    }
#endif
    [[nodiscard]] uint8_t type() const noexcept
    {
        return m_data[m_tokens[2].position];
    }

    template <uint16_t Tag>
    std::expected<std::span<const uint8_t>, ParserStatus> getString(const bool required)
    {
        const auto token = next(Tag);
        if (token == nullptr && required)
        {
            return std::unexpected{ParserStatus::RequiredFieldMissing};
        }
        return m_data.subspan(token->position, token->length);
    }

    template <uint16_t Tag>
    std::expected<uint32_t, ParserStatus> getUnsigned(const bool required)
    {
        const auto token = next(Tag);
        if (token == nullptr && required)
        {
            return std::unexpected(ParserStatus::RequiredFieldMissing);
        }
        return convertToUnsigned(token);
    }

    Tokenizer::Token* next(const uint16_t tag)
    {  // assume that fields are access once in tag order
        const auto tokens = m_tokens;
        BitSet64 present{m_present};
        while (!present.empty())
        {
            const int32_t position = present.zerosRight();
            if (tokens[position].tag == tag)
            {
                m_present.clear(position);
                return &tokens[position];
            }
            present.clear(position);
        }
        return nullptr;
    }

#if 0
    Tokenizer::Token* next(const int32_t position)
    {  // assume that fields are access once in tag order
        // m_position should reside here
        if (m_tokens.size() >= position || m_present.get(position) == 0)
        {
            return nullptr;
        }
        m_present.clear(position);
        return &m_tokens[position];
    }
#endif
    uint32_t convertToUnsigned(const Token* token) const
    {
        return asciiToDecimal(m_data.data() + token->position, token->length);
    }

    std::span<const uint8_t> convertToSpan(const Token* token) const
    {
        return m_data.subspan(token->position, token->length);
    }

};
}

#endif //SIMD_FIX_MESSAGE_HPP
