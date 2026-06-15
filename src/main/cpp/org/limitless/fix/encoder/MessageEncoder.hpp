//
// Created by Fredrik Dahlberg on 2026-06-11.
//

#ifndef SIMD_FIX_MESSAGE_ENCODER_HPP
#define SIMD_FIX_MESSAGE_ENCODER_HPP

#include "org/limitless/fix/encoder/FieldEncoder.hpp"

namespace org::limitless::fix::encoder {

class MessageEncoder
{
protected:
    FieldEncoder m_encoder;

public:
    MessageEncoder() = default;

    void wrap(const std::span<uint8_t> data, const size_t offset = 0)
    {
        m_encoder.wrap(data, offset);
    }

    /**
     * @return the number of bytes written since the last wrap()
     */
    [[nodiscard]] size_t encodedLength() const
    {
        return m_encoder.encodedLength();
    }
};

}

#endif //SIMDFIX_MESSAGE_ENCODER_HPP
