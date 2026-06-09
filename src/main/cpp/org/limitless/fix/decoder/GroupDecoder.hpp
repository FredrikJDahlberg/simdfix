//
// Created by Fredrik Dahlberg on 2026-04-26.
//

#ifndef SIMD_FIX_GROUP_DECODER_HPP
#define SIMD_FIX_GROUP_DECODER_HPP

#include "org/limitless/fix/decoder/FieldDecoder.hpp"

namespace org::limitless::fix::decoder {

struct GroupDecoder
{
protected:
    FieldDecoder& m_decoder;

    std::span<Token> m_group{};

    int32_t m_count{};
    int32_t m_repeat{};
    uint16_t m_delim{};
    uint32_t m_offset{};

public:

    explicit GroupDecoder(FieldDecoder& decoder) : m_decoder(decoder)
    {
    }

    GroupDecoder& wrap(const uint32_t tag)
    {
        m_decoder.m_group = true;  // FIXME
        m_group = m_decoder.m_tokens;
        const Token* token = next(tag);
        if (token != nullptr)
        {
            m_offset = token - &m_group[0];
            m_count = m_decoder.convertToUint32(token);
            m_delim = m_group[m_offset + 1].tag;
            m_repeat = 0;
        }
        else
        {
            throw std::invalid_argument("GroupDecoder::wrap: tag not found");
        }
        return *this;
    }

    [[nodiscard]] bool hasNext() const
    {
        auto doWork = m_repeat < m_count;
        m_decoder.m_group = doWork;
        return doWork;
    }

    void next()
    {
        m_offset = m_decoder.nextGroup(m_offset + 1, m_delim);
        ++m_repeat;
    }

    void clear()
    {
        m_count = 0;
        m_repeat = 0;
        m_offset = 0;
    }

    [[nodiscard]] uint32_t count() const
    {
        return m_count;
    }

protected:
    [[nodiscard]] Token* next(const uint32_t tag)
    {
        return m_decoder.nextField(tag);
    }

    [[nodiscard]] const Token* next(const uint32_t tag) const
    {
        return m_decoder.nextField(tag);
    }

};

}

#endif //SIMD_FIX_GROUP_DECODER_HPP
