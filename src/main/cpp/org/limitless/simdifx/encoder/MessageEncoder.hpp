//
// Created by Fredrik Dahlberg on 2026-06-11.
//

#ifndef SIMD_FIX_MESSAGE_ENCODER_HPP
#define SIMD_FIX_MESSAGE_ENCODER_HPP

#include "../detail/encoder/FieldEncoder.hpp"

namespace org::limitless::simdifx::encoder {

using namespace org::limitless::simdifx::detail::encoder;

// Base class for generated message encoders (e.g. LogonEncoder, HeartbeatEncoder).
// Owns the FieldEncoder used to write the message body and tracks how many bytes
// have been encoded since wrap().
class MessageEncoder
{
protected:
    FieldEncoder m_encoder;

public:
    MessageEncoder() = default;

    /**
     * Rebinds the encoder to begin writing the message body into data at offset.
     * @param data destination buffer
     * @param offset byte offset within data at which the message body begins
     */
    void wrap(const std::span<uint8_t> data, const uint32_t offset = 0)
    {
        m_encoder.wrap(data, offset);
    }

    /**
     * @return the number of bytes written since the last wrap()
     */
    [[nodiscard]] uint32_t encodedLength() const
    {
        return m_encoder.encodedLength();
    }
};

}

#endif //SIMD_FIX_MESSAGE_ENCODER_HPP
