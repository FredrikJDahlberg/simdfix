//
// Created by Fredrik Dahlberg on 2026-04-26.
//

#ifndef SIMD_FIX_GROUP_DECODER_HPP
#define SIMD_FIX_GROUP_DECODER_HPP

#include "org/limitless/fix/decoder/Dictionary.hpp"
#include "org/limitless/fix/decoder/Tokens.hpp"
#include "org/limitless/fix/utils/Utils.hpp"
//#include "org/limitless/fix/simd/LinearSearch.hpp"

namespace org::limitless::fix::decoder {

struct GroupDecoder
{
protected:
    Tokens* m_decoder;

    std::span<Token> m_group{};

    int32_t m_count{};
    int32_t m_repeat{};
    uint16_t m_delim{};
    uint32_t m_offset{};

public:

    GroupDecoder() : m_decoder(nullptr)
    {
    }

    [[nodiscard]] bool hasNext() const
    {
        return m_repeat < m_count;
    }

    GroupDecoder& wrap(Tokens* decoder, const uint32_t tag)
    {
        m_decoder = decoder;
        const Token* token = next(tag);
        if (token != nullptr)
        {
            m_offset = token - m_group.data();
            m_count = m_decoder->convertToUint32(token);
            m_delim = m_group[m_offset + 1].tag;
            m_repeat = 0;
            m_offset = 0;
        }
        else
        {
            std::cout << "Tag not found = " << tag << std::endl;
        }
        return *this;
    }

    void clear()
    {
        m_count = 0;
        m_repeat = 0;
        m_offset = 0;
    }

    [[nodiscard]] Token* member(const int32_t tag) const
    {
        const auto tokens = m_decoder->m_tokens;
        const size_t  size = tokens.size();
        uint32_t end = m_offset + 1;
        while (end < size && tokens[end].tag != m_delim)
        {
            ++end;
        }

        const auto position = simd::find(m_decoder->m_tags.data() + m_offset, end - m_offset, tag);
        return position >= 0 ? &m_group[m_offset + position] : nullptr;
    }

    [[nodiscard]] Token* next(const uint32_t tag)
    {
        return m_decoder->next(tag);
    }

    [[nodiscard]] const Token* next(const uint32_t tag) const
    {
        return m_decoder->next(tag);
    }

    void next()
    {
        auto tokens = m_decoder->m_tokens;
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
#if 0
    template <int32_t Tag>
    [[nodiscard]] std::expected<utils::String, Result::Values> getString(const bool required) const
    {
        return m_decoder->template getString<Tag>(required);
    }

    template <int32_t Tag>
    [[nodiscard]] std::expected<uint32_t, Result::Values> getUnsigned(const bool required) const
    {
        return m_decoder->template getUnsigned<Tag>(required);
    }
#endif
};

}

#endif //SIMD_FIX_GROUP_DECODER_HPP
