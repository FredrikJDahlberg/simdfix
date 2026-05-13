//
// Created by Fredrik Dahlberg on 2026-04-24.
//

#ifndef SIMD_FIX_MESSAGE_DECODER_HPP
#define SIMD_FIX_MESSAGE_DECODER_HPP

#include <span>
#include <expected>

#include "org/limitless/fix/parser/Token.hpp"
#include "org/limitless/fix/parser/ParserStatus.hpp"
#include "org/limitless/fix/parser/Tokenizer.hpp"
#include "org/limitless/fix/utils/Utils.hpp"
#include "org/limitless/fix/utils/BitSet64.hpp"
#include "org/limitless/fix/simd/QuadSearch.hpp"

namespace org::limitless::fix::parser {

template <typename Protocol>
struct MessageDecoder
{
    using String = std::span<const uint8_t>;

    std::span<const uint8_t> m_data{};
    std::span<Token> m_tokens{};
    const uint16_t* m_tags;

    // required fields access more than once will be cached
    // FIXME: cache all parsed fields?
    String m_sender{};           // FIXME: configuration and verification
    String m_target{};           // FIXME: configuration and verification
    String m_sendingTime{};
    uint32_t m_sequenceNumber{};

    static constexpr auto MessageGrammar = Protocol::Grammar;

    MessageDecoder() = default;

    explicit MessageDecoder(const std::span<Token> tokens, const uint16_t* tags)
        : m_tokens{tokens}, m_tags{tags}
    {
    }

    MessageDecoder(const String data, const std::span<Token> tokens, const uint16_t* tags)
        : m_data{data}, m_tokens{tokens}, m_tags{tags}
    {
    }

    void wrap(const String data, const std::span<Token> tokens, const uint16_t* tags)
    {
        m_data = data;
        m_tokens = tokens;
        m_tags = tags;
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
        //return MessageGrammar.lookup(tag).value().type;
        throw std::invalid_argument("not implemented");
        return 0;
    }

    template <int32_t Tag>
    std::expected<String, ParserStatus> getString(const bool required) const
    {
        const auto token = next(Tag);
        if (token == nullptr)
        {
            return std::unexpected{required ? ParserStatus::RequiredFieldMissing : ParserStatus::NullValue};
        }
        return m_data.subspan(token->position, token->length);
    }

    template <int32_t Tag>
    std::expected<uint32_t, ParserStatus> getUnsigned(const bool required) const
    {
        const auto token = next(Tag);
        if (token == nullptr)
        {
            return std::unexpected{required ? ParserStatus::RequiredFieldMissing : ParserStatus::NullValue};
        }
        return convertToUnsigned(token);
    }

    [[nodiscard]] Token* next(const int32_t tag) const
    {
        auto index = simd::quadSearch(m_tags, m_tokens.size(), tag);
        if (index >= 0)
        {
            return &m_tokens[index];
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
        return utils::asciiToDecimal(0, m_data.data() + token->position, token->length);
    }

    std::span<const uint8_t> convertToSpan(const Token* token) const
    {
        return m_data.subspan(token->position, token->length);
    }

    [[nodiscard]] ParserStatus checkRequired()
    {
        if (const auto sender = this->getString<49>(true))
        {
            m_sender = sender.value();
        }
        else
        {
            return ParserStatus::InvalidSenderCompId;
        }
        if (const auto target = this->getString<56>(true))
        {
            m_target = target.value();
        }
        else
        {
            return ParserStatus::InvalidTargetCompId;
        }
        if (const auto sequenceNumber = getUnsigned<34>(true))
        {
            m_sequenceNumber = sequenceNumber.value();
        }
        else
        {
            return ParserStatus::InvalidSequenceNumber;
        }
        if (const auto sendingTime = getString<52>(true))
        {
            m_sendingTime = sendingTime.value();
        }
        else
        {
            return ParserStatus::InvalidSendingTime;
        }
        return ParserStatus::Success;
    }
};
}

#endif //SIMD_FIX_MESSAGE_HPP
