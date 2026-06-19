//
// Created by Fredrik Dahlberg on 2026-04-26.
//

#ifndef SIMD_FIX_GROUP_DECODER_HPP
#define SIMD_FIX_GROUP_DECODER_HPP

#include "org/limitless/fix/decoder/FieldDecoder.hpp"

namespace org::limitless::fix::decoder {

/**
 * Iterates the repeating entries of a FIX repeating group. Wraps the
 * NumInGroup count field, then advances entry-by-entry, pushing a
 * FieldDecoder group scope for each entry so that getString/getUint32/...
 * with ParentType::Group resolve against the current entry only.
 */
class GroupDecoder
{
private:
    uint32_t m_count{};
    uint32_t m_repeat{};
    uint16_t m_delim{};
    uint32_t m_offset{};

protected:
    FieldDecoder& m_decoder;

public:
    /**
     * @param decoder field decoder over the message containing this group
     */
    explicit GroupDecoder(FieldDecoder& decoder) : m_decoder(decoder)
    {
    }

    GroupDecoder(const GroupDecoder&) = delete;
    GroupDecoder& operator=(const GroupDecoder&) = delete;
    GroupDecoder(GroupDecoder&&) = delete;
    GroupDecoder& operator=(GroupDecoder&&) = delete;

    /**
     * Locates the NumInGroup count field for Tag and resets iteration. The
     * tag immediately following it is taken as the group's delimiter
     * (first field of each repeating entry).
     * @tparam Tag NumInGroup tag number for this group
     * @return this decoder
     * @throws std::invalid_argument if Tag is not found
     */
    template <uint32_t Tag>
    GroupDecoder& wrap()
    {
        const Token* token = m_decoder.nextField(Tag);
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

    /**
     * @return true if there are more entries to iterate via next()
     */
    [[nodiscard]] bool hasNext() const
    {
        return m_repeat < m_count;
    }

    /**
     * Advances to the next repeating-group entry, replacing the current
     * FieldDecoder group scope (if any) with the new entry's
     * [begin, end) token range.
     */
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

    /**
     * Leaves the current group scope, if any, and resets iteration state.
     */
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

    /**
     * @return the NumInGroup value from the most recent wrap()
     */
    [[nodiscard]] uint32_t count() const
    {
        return m_count;
    }

private:
    /**
     * Finds the token index where the next repeating-group entry begins:
     * the next occurrence of the delimiter tag after the current offset,
     * bounded by the enclosing group scope (or the whole message if this
     * group is not nested).
     * @return token/tag index of the next entry's delimiter field
     */
    int32_t nextGroupOffset() const
    {
        const auto outerEnd = m_decoder.groupScope().end;
        const auto end = m_decoder.find(m_offset + 1, m_delim, outerEnd);
        return m_decoder.indexOf(end);
    }
};

}

#endif //SIMD_FIX_GROUP_DECODER_HPP
