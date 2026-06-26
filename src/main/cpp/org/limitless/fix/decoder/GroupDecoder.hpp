//
// Created by Fredrik Dahlberg on 2026-04-26.
//

#ifndef SIMD_FIX_GROUP_DECODER_HPP
#define SIMD_FIX_GROUP_DECODER_HPP

#include "../detail/parser/FieldDecoder.hpp"

namespace org::limitless::fix::decoder {

using namespace org::limitless::fix::detail::parser;

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
    uint32_t m_next{};

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
        const auto* field = m_decoder.nextField(Tag);
        if (field != nullptr)
        {
            if (m_repeat > 0)
            {
                m_decoder.popGroupScope();
            }
            m_offset = m_decoder.indexOf(field);
            const auto count = m_decoder.convertToUint32(field);
            m_count = count.value_or(0);
            m_delim = m_decoder.fieldAt(m_offset + 1).m_tag;
            m_repeat = 0;
            // Locate the first entry's delimiter once; next() consumes this and
            // each entry's end becomes the following entry's start.
            if (m_count > 0)
            {
                m_next = nextGroupOffset();
            }
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
     * [begin, end) field range. The entry's start is the previous entry's
     * cached end (or the first delimiter from wrap()), so only a single
     * delimiter scan per entry is needed to find the end.
     */
    void next()
    {
        if (m_repeat > 0)
        {
            m_decoder.popGroupScope();
        }
        m_offset = m_next;
        ++m_repeat;

        m_next = nextGroupOffset();
        m_decoder.pushGroupScope(m_offset, m_next);
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
     * Finds the field index where the next repeating-group entry begins:
     * the next occurrence of the delimiter tag after the current offset,
     * bounded by the enclosing group scope (or the whole message if this
     * group is not nested).
     * @return field/tag index of the next entry's delimiter field
     */
    [[nodiscard]] int32_t nextGroupOffset() const
    {
        const auto outerEnd = m_decoder.groupScope().end;
        const auto end = m_decoder.find(m_offset + 1, m_delim, outerEnd);
        return m_decoder.indexOf(end);
    }
};

}

#endif //SIMD_FIX_GROUP_DECODER_HPP
