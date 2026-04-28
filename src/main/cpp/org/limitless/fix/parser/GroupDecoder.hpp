//
// Created by Fredrik Dahlberg on 2026-04-26.
//

#ifndef SIMD_FIX_GROUP_DECODER_HPP
#define SIMD_FIX_GROUP_DECODER_HPP

namespace org::limitless::fix::parser {

#include "org/limitless/fix/parser/Tokenizer.hpp"

template <typename Grammar>
struct GroupDecoder
{
    using Token = Tokenizer::Token;
    using base_t = MessageDecoder<Grammar>;

protected:
    const base_t* m_message;
    Token* m_groupCount{};
    Token* m_group{};

    uint16_t m_delim{};

    uint32_t m_count{};
    int32_t m_position{};
    int32_t m_offset{};

public:
    explicit GroupDecoder(const base_t* message) : m_message(message)
    {
    }

    GroupDecoder& wrap() // MessageDecoder<Grammar>* message
    {
        //m_message = message;
        //return *this;
    }

    [[nodiscard]] bool hasNext() const
    {
        return m_position < m_count;
    }

    void check(Token* token)
    {
        if (token != nullptr)
        {
            m_group = token;
            m_offset = static_cast<int32_t>(m_group - &m_message->m_tokens[0]);
            m_position = 0;
            m_count = m_message->convertToUnsigned(m_group);
            m_delim = m_message->m_tokens[m_offset + 1].tag; // FIXME
        }
    }

    void clear()
    {
        // FIXME:
        m_count = 0;
        m_position = 0;
        m_offset = 0;
    }

    void next()
    {
        ++m_offset;
        ++m_group;

        if (m_group->tag == m_delim)
        {
            ++m_position;
        }
        // FIXME
        // if (m_group->tag != Type)
        {

        }

        //m_group = m_message->next(m_offset + m_position);
        ++m_position;
    }

    [[nodiscard]] size_t count() const
    {
        if (m_groupCount == nullptr)
        {
            return 0;
        }
        return m_count;
    }
};

}

#endif //SIMD_FIX_GROUP_DECODER_HPP
