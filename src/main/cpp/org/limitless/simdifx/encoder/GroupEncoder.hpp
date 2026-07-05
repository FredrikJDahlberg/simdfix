//
// Created by Fredrik Dahlberg on 2026-06-11.
//

#ifndef SIMD_FIX_GROUP_ENCODER_HPP
#define SIMD_FIX_GROUP_ENCODER_HPP

#include "../detail/encoder/FieldEncoder.hpp"

namespace org::limitless::simdifx::encoder {

using namespace org::limitless::simdifx::detail::encoder;

// Base class for generated repeating-group encoders (e.g. HopsEncoder).
// Shares the FieldEncoder of the enclosing MessageEncoder/GroupEncoder and
// writes the group's counter field on wrap(), then tracks the current entry
// as next() advances through the group.
class GroupEncoder
{
public:

    GroupEncoder() = delete;

    /**
     * Constructs a group encoder sharing the given FieldEncoder.
     * @param encoder field encoder of the enclosing MessageEncoder/GroupEncoder
     */
    GroupEncoder(FieldEncoder& encoder) : m_encoder{encoder}
    {
    }

    /**
     * Writes the group's counter field (e.g. "627=3") and resets the entry
     * index so the first call to next() moves to entry 0.
     * @param tag tag number of the group's counter field
     * @param count number of entries in the group
     */
    void wrap(const uint32_t tag, const uint32_t count)
    {
        m_encoder.encodeField(tag, count);
        m_count = 0;
    }

    /**
     * Advances to the next group entry. Must be called before encoding the
     * fields of each entry, including the first.
     */
    void next()
    {
        ++m_count;
    }

protected:

    FieldEncoder& m_encoder;
    int32_t m_count;
};

}

#endif //SIMD_FIX_GROUP_ENCODER_HPP
