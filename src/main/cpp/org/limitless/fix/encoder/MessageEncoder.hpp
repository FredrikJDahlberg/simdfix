//
// Created by Fredrik Dahlberg on 2026-06-11.
//

#ifndef SIMD_FIX_MESSAGE_ENCODER_HPP
#define SIMD_FIX_MESSAGE_ENCODER_HPP

#include "org/limitless/fix/encoder/FieldEncoder.hpp"

namespace org::limitless::fix::messages::encoder {

struct MessageEncoder
{
    FieldEncoder m_encoder;
};

}

#endif //SIMDFIX_MESSAGE_ENCODER_HPP
