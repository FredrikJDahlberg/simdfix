//
// Created by Fredrik Dahlberg on 2026-06-05.
//

#ifndef SIMD_FIX_STRUCT_DECODER_HPP
#define SIMD_FIX_STRUCT_DECODER_HPP

#include "org/limitless/fix/decoder/Tokens.hpp"

namespace org::limitless::fix::decoder {

struct StructDecoder
{
protected:
    const Tokens* m_decoder;

public:

    StructDecoder() : m_decoder(nullptr)
    {
    }

    explicit StructDecoder(const Tokens* decoder) : m_decoder(decoder)
    {
    }

    StructDecoder& wrap(const Tokens* decoder)
    {
        m_decoder = decoder;
        return *this;
    }
};

}
#endif //SIMD_FIX_STRUCT_DECODER_HPP
