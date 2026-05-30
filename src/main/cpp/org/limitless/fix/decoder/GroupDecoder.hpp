//
// Created by Fredrik Dahlberg on 2026-04-26.
//

#ifndef SIMD_FIX_GROUP_DECODER_HPP
#define SIMD_FIX_GROUP_DECODER_HPP

#include "org/limitless/fix/utils/Utils.hpp"
#include "org/limitless/fix/simd/LinearSearch.hpp"

namespace org::limitless::fix::decoder {

template <typename Message>
struct GroupDecoder
{
protected:
    const Message* m_message;
    std::span<Token> m_tokens{};

    uint32_t m_count{};
    int32_t m_repeat{};
    uint16_t m_delim{};
    uint32_t m_offset{};

public:
    GroupDecoder() : m_message(nullptr)
    {
    }

    explicit GroupDecoder(const Message* grammar) : m_message(grammar)
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

    void wrap(const Token* token)
    {
        m_offset = token - m_tokens.data();
        m_count = m_message->convertToUnsigned(&m_tokens[m_offset]);
        m_delim = m_tokens[m_offset + 1].tag;
        m_repeat = 0;
        m_offset = 0;
    }

    void clear()
    {
        m_count = 0;
        m_repeat = 0;
        m_offset = 0;
    }

    [[nodiscard]] Token* member(const int32_t tag)
    {
        const auto tokens = m_message->m_tokens;
        const size_t  size = tokens.size();
        uint32_t end = m_offset + 1;
        while (end < size && tokens[end].tag != m_delim)
        {
            ++end;
        }
        auto position = simd::find(m_message->m_tags.data() + m_offset, end - m_offset, tag);
        return position >= 0 ? &m_message->m_tokens[m_offset + position] : nullptr;
    }

    [[nodiscard]] Token* next(const uint32_t tag) const
    {
        return m_message->next(tag);
    }

    void next()
    {
        auto tokens = m_message->m_tokens;
        ++m_offset;
        while (m_offset < tokens.size() && tokens[m_offset].tag != m_delim)
        {
            ++m_offset;
        }
        ++m_repeat;
    }

    [[nodiscard]] uint32_t count() const
    {
        return m_count;
    }

    // FIXME: restructure
    template <int32_t Tag>
    [[nodiscard]] std::expected<utils::String, DecoderStatus> getString(const bool required) const
    {
        return m_message->template getString<Tag>(required);
    }

    template <int32_t Tag>
    [[nodiscard]] std::expected<uint32_t, DecoderStatus> getUnsigned(const bool required) const
    {
        return m_message->template getUnsigned<Tag>(required);
    }
};

}

#endif //SIMD_FIX_GROUP_DECODER_HPP
