//
// Created by Fredrik Dahlberg on 2026-06-05.
//

#ifndef SIMD_FIX_STRUCT_DECODER_HPP
#define SIMD_FIX_STRUCT_DECODER_HPP

#include "org/limitless/fix/decoder/FieldDecoder.hpp"

namespace org::limitless::fix::decoder {

struct ComponentDecoder
{
protected:
    FieldDecoder& m_decoder;

public:

    explicit ComponentDecoder(FieldDecoder& decoder) : m_decoder{decoder}
    {
    }
};

}
#endif //SIMD_FIX_STRUCT_DECODER_HPP
