//
// Created by Fredrik Dahlberg on 2026-04-26.
//

#ifndef SIMD_FIX_GROUP_DECODER_HPP
#define SIMD_FIX_GROUP_DECODER_HPP

#include "FieldDecoder.hpp"

namespace org::limitless::fix::decoder {

struct GroupDecoder
{
protected:
    FieldDecoder& m_decoder;

    uint32_t m_count{};
    uint32_t m_repeat{};
    uint16_t m_delim{};
    uint32_t m_offset{};

public:

    explicit GroupDecoder(FieldDecoder& decoder) : m_decoder(decoder)
    {
    }

    GroupDecoder& wrap(const uint32_t tag)
    {
        const Token* token = m_decoder.nextField(tag);
        if (token != nullptr)
        {
            if (m_repeat > 0)
            {
                m_decoder.popGroupScope();
            }
            m_offset = m_decoder.indexOf(token);
            m_count = m_decoder.convertToUint32(token);
            m_delim = m_decoder.tokenAt(m_offset + 1).m_tag;
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
        return m_repeat < m_count;
    }

    void next()
    {
        if (m_repeat > 0)
        {
            m_decoder.popGroupScope();
        }
        m_offset = nextGroupOffset();
        ++m_repeat;

        const auto found = nextGroupOffset();
        m_decoder.pushGroupScope(m_offset, found);
    }

    void clear()
    {
        if (m_repeat > 0)
        {
            m_decoder.popGroupScope();
        }
        m_count = 0;
        m_repeat = 0;
        m_offset = 0;
    }

    [[nodiscard]] uint32_t count() const
    {
        return m_count;
    }

private:
    int32_t nextGroupOffset() const
    {
        const auto outerEnd = m_decoder.groupScope().end;
        const auto end = m_decoder.find(m_offset + 1, m_delim, outerEnd);
        return m_decoder.indexOf(end);
    }
};

}

#endif //SIMD_FIX_GROUP_DECODER_HPP
