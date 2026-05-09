//
// Created by Fredrik Dahlberg on 2026-04-26.
//

#ifndef SIMD_FIX_GROUP_DECODER_HPP
#define SIMD_FIX_GROUP_DECODER_HPP

namespace org::limitless::fix::parser {

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
    int32_t m_repeat{};
    //int32_t m_offset{};

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
        //auto tag = m_message->m_tokens[m_offset].tag;
        auto tag = m_group->tag;
        auto tokenType = m_message->tokenType(tag);
        std::printf("HAS NEXT: TYPE TAG = %d TYPE = %d / %d\n", m_group->tag, tokenType, m_groupType);
        if (m_groupType != tokenType)
        {
            std::printf("DONE\n");
            return false;
        }

        return m_repeat < m_count;
    }

    void prepare(Token* token)
    {
        m_group = token;
        //m_offset = static_cast<int32_t>(m_group - &m_message->m_tokens[0]);
        m_repeat = 0;
        m_count = m_message->convertToUnsigned(m_group);
        m_delim = (m_group + 1)->tag; // m_message->m_tokens[m_offset + 1].tag;
        std::printf("COUNT = %d, DELIM = %d, GROUP = %d\n", m_count, m_delim, m_group - &m_message->m_tokens[0]);
    }

    void clear()
    {
        m_count = 0;
        m_repeat = 0;
        // m_offset = 0;
    }

    [[nodiscard]] Token* next(const int32_t tag, const int32_t sentinel)
    {  // assume that fields are access once in tag order
        const auto tokens = m_message->m_tokens;
        BitSet64 present{m_message->m_present};
        if (present.empty())
        {
            return nullptr;
        }
        int32_t position = present.zerosRight();

        // tokens[] = { 627, 628, 629, 628, 629 }
        // first: pointing to count
        // next -> pointing to first rep
        // next ->
        ++m_group;
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
        auto size = m_message->m_tokens.size();
        std::printf("NEXT: pos = %d, tag = %d\n", m_group - &m_message->m_tokens[0], m_group->tag);
        while (m_group - m_message->m_tokens.data() < size && m_group->tag != m_delim)
        {
            // ++m_offset;
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
