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
    FieldEncoder m_encoder{};

public:
    MessageEncoder() = default;

    void wrap(const std::span<uint8_t> data)
    {
        m_encoder.wrap(data);
    }
};

}

#endif //SIMDFIX_MESSAGE_ENCODER_HPP
