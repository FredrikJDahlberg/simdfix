//
// Created by Fredrik Dahlberg on 2026-06-11.
//

#ifndef SIMD_FIX_PAYLOAD_ENCODER_HPP
#define SIMD_FIX_PAYLOAD_ENCODER_HPP

#include <cstdint>
#include "org/limitless/fix/CodecTypes.hpp"

namespace org::limitless::fix::encoder {

class PayloadEncoder
{

    uint8_t m_protocol[16];
    uint32_t m_protocolLength;

public:
    explicit PayloadEncoder(const Protocol::Values protocol)
    {
        m_protocol[0] = '8';
        m_protocol[1] = '=';
        const auto code = Protocol::code(protocol);
        const auto size = code.size();
        std::memcpy(m_protocol + 2, code.data(), size);
        m_protocol[2 + size] = '\001'; // FIXME
        m_protocolLength = size + 2;
    }

};

}

#endif //SIMD_FIX_PAYLOAD_ENCODER_HPP
