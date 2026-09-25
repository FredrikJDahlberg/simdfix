//
// Created by Fredrik Dahlberg on 2026-04-26.
//

#ifndef SIMD_FIX_GROUP_DECODER_HPP
#define SIMD_FIX_GROUP_DECODER_HPP

#include "../detail/parser/FieldDecoder.hpp"

namespace org::limitless::simdfix::decoder {

using namespace org::limitless::simdfix::detail::parser;

/**
 * Iterates the repeating entries of a FIX repeating group. Wraps the
 * NumInGroup count field, then advances entry-by-entry, pushing a
 * FieldDecoder group scope for each entry so that getString/getUint32/...
 * with ParentType::Group resolve against the current entry only.
 *
 * Each group sits at a nesting depth: the number of enclosing group entries,
 * 0 for a group directly in the message. Its entry scopes live at that depth
 * of the FieldDecoder's scope stack, so wrapping or advancing it discards any
 * scopes left by groups nested inside its previous entry.
 */
class GroupDecoder
{
private:
    uint32_t m_count{};
    uint32_t m_repeat{};
    uint16_t m_delim{};
    uint32_t m_offset{};
    uint32_t m_next{};
    int32_t m_depth{};

protected:
    FieldDecoder& m_decoder;

    /**
     * @return this group's nesting depth, which its field getters pass to the
     *         FieldDecoder so they read this group's current entry even while a
     *         group nested in it is being iterated
     */
    [[nodiscard]] int32_t scopeDepth() const
    {
        return m_depth;
    }

    /**
     * @return the nesting depth of a group inside this group's current entry
     */
    [[nodiscard]] int32_t childDepth() const
    {
        return m_depth + 1;
    }

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
     * Locates the NumInGroup count field for Tag within the enclosing scope
     * and resets iteration. The tag immediately following it is taken as the
     * group's delimiter (first field of each repeating entry). An absent group
     * reads as empty: count() is 0 and hasNext() is false.
     * @tparam Tag NumInGroup tag number for this group
     * @param depth nesting depth: 0 for a group in the message, or the enclosing
     *        group's childDepth() for a nested one
     * @return this decoder
     */
    template <uint32_t Tag>
    GroupDecoder& wrap(const int32_t depth = 0)
    {
        m_depth = depth;
        m_decoder.truncateGroupScopes(m_depth);
        m_count = 0;
        m_repeat = 0;
        m_offset = 0;
        const auto [begin, end] = m_decoder.groupScope();
        const auto* field = m_decoder.find(begin, static_cast<uint16_t>(Tag), end);
        if (m_decoder.indexOf(field) < end)
        {
            m_offset = m_decoder.indexOf(field);
            const auto count = m_decoder.convertToUint32(field);
            m_count = count.value_or(0);
            m_delim = m_decoder.fieldAt(m_offset + 1).m_tag;
            // Locate the first entry's delimiter once; next() consumes this and
            // each entry's end becomes the following entry's start.
            if (m_count > 0)
            {
                m_next = nextGroupOffset();
            }
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
     * delimiter scan per entry is needed to find the end. If the scope cannot
     * be entered (nesting deeper than MaxGroupDepth), iteration ends instead.
     */
    void next()
    {
        m_decoder.truncateGroupScopes(m_depth);
        m_offset = m_next;
        m_next = nextGroupOffset();
        if (!m_decoder.pushGroupScope(m_offset, m_next)) [[unlikely]]
        {
            m_count = 0;
            m_repeat = 0;
            return;
        }
        ++m_repeat;
    }

    /**
     * Leaves the current entry's scope, and those of any groups nested in it,
     * and resets iteration state.
     */
    void clear()
    {
        if (m_repeat > 0)
        {
            m_decoder.truncateGroupScopes(m_depth);
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
