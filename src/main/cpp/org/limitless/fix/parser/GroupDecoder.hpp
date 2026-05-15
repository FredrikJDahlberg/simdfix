//
// Created by Fredrik Dahlberg on 2026-04-26.
//

#ifndef SIMD_FIX_GROUP_DECODER_HPP
#define SIMD_FIX_GROUP_DECODER_HPP

#include "org/limitless/fix/simd/QuadSearch.hpp"

namespace org::limitless::fix::parser {

template <typename Message>
struct GroupDecoder
{
protected:
    const Message* m_message;
    const uint32_t m_groupType;

    Token* m_groupCount{};
    Token* m_group{};

    uint32_t m_count{};
    int32_t m_repeat{};
    uint16_t m_delim{};

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
        return m_repeat < m_count;
    }

    void prepare(Token* token)
    {
        m_group = token;
        m_repeat = 0;
        m_count = m_message->convertToUnsigned(m_group);
        m_delim = (m_group + 1)->tag;
    }

    void clear()
    {
        m_count = 0;
        m_repeat = 0;
    }

    [[nodiscard]] Token* findInHop(const int32_t tag)
    {
        const Token* base = m_message->m_tokens.data();
        const size_t  size = m_message->m_tokens.size();
        const uint32_t offset = static_cast<uint32_t>(m_group - base);
        uint32_t end = offset + 1;
        while (end < size && base[end].tag != m_delim)
        {
            ++end;
        }

        const int32_t len = static_cast<int32_t>(end - offset);
        const int32_t position = simd::quadSearch(m_message->m_tags.data() + offset, len, tag);
        return position >= 0 ? &m_message->m_tokens[offset + position] : nullptr;
    }

    [[nodiscard]] Token* next(const uint32_t tag) const
    {
        return m_message->next(static_cast<uint16_t>(tag));
    }

    void next()
    {
        const size_t size = m_message->m_tokens.size();
        ++m_group;
        while (m_group - m_message->m_tokens.data() < size && m_group->tag != m_delim)
        {
            ++m_group;
        }
        ++m_repeat;
    }

    [[nodiscard]] size_t count() const
    {
        return m_count;
    }
};

}

#endif //SIMD_FIX_GROUP_DECODER_HPP
