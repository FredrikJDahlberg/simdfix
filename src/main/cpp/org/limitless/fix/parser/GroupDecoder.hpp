//
// Created by Fredrik Dahlberg on 2026-04-26.
//

#ifndef SIMD_FIX_GROUP_DECODER_HPP
#define SIMD_FIX_GROUP_DECODER_HPP

namespace org::limitless::fix::parser {

#include "org/limitless/fix/parser/Token.hpp"

template <typename Message>
struct GroupDecoder
{
protected:
    const Message* m_message;
    const uint32_t m_groupType;

    Token* m_groupCount{};
    Token* m_group{};

    uint16_t m_delim{};

    uint32_t m_count{};
    int32_t m_position{};
    int32_t m_offset{};

public:
    explicit GroupDecoder(const uint32_t groupType, const Message* grammar) : m_message(grammar), m_groupType(groupType)
    {
    }

    GroupDecoder& wrap()
    {
        return *this;
    }

    [[nodiscard]] bool hasNext() const
    {
        auto tag = m_message->m_tokens[m_offset].tag;
        auto tokenType = m_message->tokenType(tag);
        std::printf("HAS NEXT: TYPE TAG = %d TYPE = %d / %d\n", m_group->tag, tokenType, m_groupType);
        if (m_groupType != tokenType)
        {
            std::printf("DONE\n");
            return false;
        }

        return m_position < m_count;
    }

    void prepare(Token* token)
    {
        m_group = token;
        m_offset = static_cast<int32_t>(m_group - &m_message->m_tokens[0]);
        m_position = 0;
        m_count = m_message->convertToUnsigned(m_group);
        m_delim = m_message->m_tokens[m_offset + 1].tag;
    }

    void clear()
    {
        m_count = 0;
        m_position = 0;
        m_offset = 0;
    }

    [[nodiscard]] Token* next(const int32_t tag, const int32_t sentinel) const
    {  // assume that fields are access once in tag order
        const auto tokens = m_message->m_tokens;
        BitSet64 present{m_message->m_present};
        if (present.empty())
        {
            return nullptr;
        }
        int32_t position = present.zerosRight();
        if (tokens[position].tag == tag)
        {
            m_message->m_present.clear(position);
            return &tokens[position];
        }
        do
        {
            if (tokens[position].tag == tag)
            {
                m_message->m_present.clear(position);
                return &tokens[position];
            }
            present.clear(position);
            position = present.zerosRight();
        } while (!present.empty() && tokens[position].tag != sentinel);
        return nullptr;
    }

    [[nodiscard]] Token* next(uint16_t tag) const
    {

        return m_message->next(tag);
    }

    void next()
    {
        //  && m_groupType == m_message->tokenType(m_group->tag)
        while (m_group->tag != m_delim)
        {
            ++m_offset;
            ++m_group;
        }
        ++m_position;
        std::printf("NEXT: TAG = %d, OFS = %d, POS = %d\n", m_group->tag, m_offset, m_position);
    }

    [[nodiscard]] size_t count() const
    {
        return m_count;
    }
};

}

#endif //SIMD_FIX_GROUP_DECODER_HPP
