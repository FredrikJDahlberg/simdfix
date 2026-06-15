//
// Created by Fredrik Dahlberg on 2026-06-11.
//

#ifndef SIMD_FIX_GROUP_ENCODER_HPP
#define SIMD_FIX_GROUP_ENCODER_HPP

#include "org/limitless/fix/encoder/FieldEncoder.hpp"

namespace org::limitless::fix::encoder {

class GroupEncoder
{
public:

    GroupEncoder() = delete;

    GroupEncoder(FieldEncoder& encoder) : m_encoder{encoder}
    {
    }

    void wrap(const uint32_t tag, const uint32_t count)
    {
        m_encoder.encodeField(tag, count);
        m_count = 0;
    }

    void next()
    {
        ++m_count;
    }

protected:

    FieldEncoder& m_encoder;
    int32_t m_count;
};

}

#endif //SIMDFIX_GROUPENCODER_HPP
