//
// Created by Fredrik Dahlberg on 2026-04-24.
//

#ifndef SIMD_FIX_MESSAGE_DECODER_HPP
#define SIMD_FIX_MESSAGE_DECODER_HPP

#include <span>
#include <expected>

#include "org/limitless/fix/parser/Dictionary.hpp"
#include "org/limitless/fix/parser/Token.hpp"
#include "org/limitless/fix/parser/ParserStatus.hpp"
#include "org/limitless/fix/parser/Tokenizer.hpp"
#include "org/limitless/fix/parser/Utils.hpp"
#include "org/limitless/fix/parser/BitSet64.hpp"
#include "org/limitless/fix/parser/PerfectHashMap.hpp"

namespace org::limitless::fix::parser {

template <typename Grammar>
struct MessageDecoder
{
    using String = std::span<const uint8_t>;

    std::span<Token> m_tokens{};
    mutable BitSet64 m_present{};

    String m_data{};
    String m_sender{};
    String m_target{};
    String m_sendingTime{};
    uint32_t m_sequenceNumber{};

    using MetaType = std::span<const Entry<Dictionary>, Grammar::Grammar.size()>;
    static constexpr auto MessageGrammar = PerfectHashMap(MetaType(Grammar::Grammar));

    MessageDecoder() = default;

    explicit MessageDecoder(const std::span<Token> tokens) : m_tokens{tokens}
    {
    }

    MessageDecoder(const String data, const std::span<Token> tokens)
        : m_data{data}, m_tokens{tokens}
    {
    }

    void wrap(const String data, const std::span<Token> tokens)
    {
        m_data = data;
        m_tokens = tokens;
        m_present.set();
        const auto size = tokens.size();
        m_present >>= m_present.capacity() - size;
        m_present.clear(0).clear(1).clear(2).clear(size - 1); // already checked
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
        return MessageGrammar.lookup(tag).value().type;
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
        return asciiToDecimal(0, m_data.data() + token->position, token->length);
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
