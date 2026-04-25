//
// Created by Fredrik Dahlberg on 2026-04-24.
//

#ifndef SIMD_FIX_MESSAGE_HPP
#define SIMD_FIX_MESSAGE_HPP

#include <span>

#include "org/limitless/fix/parser/Tokenizer.hpp"
#include "org/limitless/fix/parser/Utils.hpp"
#include "org/limitless/fix/parser/BitSet64.hpp"

namespace org::limitless::fix::parser {

struct Message
{
    using Token = Tokenizer::Token;

    std::span<const uint8_t> m_data{};
    std::span<Token> m_tokens{};
    BitSet64 m_present{};

    // FIXME: nested group stack

    static constexpr std::span<uint8_t> NullSpan{};

    Message(const std::span<const uint8_t> data, const std::span<Token> tokens, BitSet64 present)
        : m_data{data}, m_tokens{tokens}, m_present{present}
    {
    }

    [[nodiscard]] uint8_t messageType() const
    {
        auto messageType = m_tokens[2];
        return m_data[messageType.position];
    }

    Tokenizer::Token* nextByTag(const uint16_t tag)
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

    Tokenizer::Token* nextByPos(int32_t position)
    {  // assume that fields are access once in tag order
        if (static_cast<int32_t>(m_tokens.size()) >= position || m_present.get(position) == 0)
        {
            return nullptr;
        }
        m_present.clear(position);
        return &m_tokens[position];
    }

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
