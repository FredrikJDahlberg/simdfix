//
// Created by Fredrik Dahlberg on 2026-06-11.
//

#ifndef SIMD_FIX_PAYLOAD_ENCODER_HPP
#define SIMD_FIX_PAYLOAD_ENCODER_HPP

#include <cstdint>
#include <cstring>
#include <span>

#include "org/limitless/fix/CodecTypes.hpp"
#include "org/limitless/fix/encoder/FieldEncoder.hpp"
#include "org/limitless/fix/simd/Uint8x16.hpp"
#include "org/limitless/fix/utils/Utils.hpp"

namespace org::limitless::fix::encoder {

// Writes the FIX header (BeginString, BodyLength, MsgType, SenderCompID, TargetCompID)
// ahead of a message body and, once the body has been encoded, fills in BodyLength
// and appends the trailing CheckSum field.
template <FixedString Protocol, FixedString Target, FixedString Sender>
class PayloadEncoder
{
    std::span<uint8_t> m_buffer{};
    uint32_t m_offset{};
    uint32_t m_encodedLength{};

    static constexpr uint32_t ProtocolLength = Protocol.Size - 1;
    static constexpr uint32_t SenderLength = Sender.Size - 1;
    static constexpr uint32_t TargetLength = Target.Size - 1;

    static constexpr uint32_t BodyLengthOffset = ProtocolLength + 5; // "8=" + protocol + SOH + "9="
    static constexpr uint32_t BodyLengthDigits = 4;
    static constexpr uint32_t MessageTypeOffset = BodyLengthOffset + BodyLengthDigits + 4; // SOH + "35="

    // Length of the header fields preceding the body, i.e. everything up to and
    // including SenderCompID/TargetCompID.
    static constexpr uint32_t HeaderLength = ProtocolLength + SenderLength + TargetLength + 23;

    // Portion of HeaderLength counted towards the FIX BodyLength (MsgType, SenderCompID, TargetCompID).
    static constexpr uint32_t HeaderBodyLength = SenderLength + TargetLength + 13;

public:
    PayloadEncoder() = default;

    /**
     * Rebinds the encoder to a destination buffer and writes the FIX header
     * fields preceding the message body (BeginString, a placeholder BodyLength,
     * a placeholder MsgType, SenderCompID, TargetCompID).
     * @param offset byte offset within buffer at which the header begins
     * @param buffer destination buffer
     * @return *this, for chaining
     */
    PayloadEncoder& wrap(const uint32_t offset, const std::span<uint8_t> buffer)
    {
        m_offset = offset;
        m_buffer = buffer;
        m_encodedLength =  FieldEncoder::encode<"8", Protocol>(m_offset, m_buffer);
        m_encodedLength += FieldEncoder::encode<"9", "0000">(m_offset + m_encodedLength, m_buffer);
        m_encodedLength += FieldEncoder::encode<"35", "?">(m_offset + m_encodedLength, m_buffer);
        m_encodedLength += FieldEncoder::encode<"49", Sender>(m_offset + m_encodedLength, m_buffer);
        m_encodedLength += FieldEncoder::encode<"56", Target>(m_offset + m_encodedLength, m_buffer);
        return *this;
    }

    /**
     * @return the offset at which the message body should be encoded
     */
    [[nodiscard]] uint32_t offset() const
    {
        return m_offset + HeaderLength;
    }

    /**
     * Wraps a message encoder around the buffer at the position where the
     * message body should be encoded.
     * @tparam MessageEncoderType message encoder type, e.g. LogonEncoder
     * @param message message encoder to wrap
     * @return message, for chaining
     */
    template <typename MessageEncoderType>
    MessageEncoderType& wrapMessage(MessageEncoderType& message) const
    {
        message.wrap(m_buffer, offset());
        return message;
    }

    /**
     * Fills in MsgType and BodyLength, and appends the CheckSum field after the
     * already-encoded message body.
     * @tparam Message message encoder type, e.g. LogonEncoder
     * @param message message whose body has already been encoded at offset()
     * @return the total number of bytes written, including header and trailer
     */
    template <EncodableMessage Message>
    uint32_t encode(const Message& message)
    {
        const auto bodyLength = HeaderBodyLength + static_cast<uint32_t>(message.encodedLength());
        m_buffer[m_offset + MessageTypeOffset] = static_cast<uint8_t>(message.type().front());
        utils::writeFixedDigits<BodyLengthDigits>(bodyLength, m_buffer.data() + m_offset + BodyLengthOffset);
        m_encodedLength = HeaderLength + static_cast<uint32_t>(message.encodedLength());

        uint32_t checksum = 0;
        uint32_t position = 0;
        simd::Uint8x16 value;
        for (; position + simd::Uint8x16::Size <= m_encodedLength; position += simd::Uint8x16::Size)
        {
            value.load(m_buffer.data() + m_offset + position);
            checksum += static_cast<uint32_t>(value.sum());
        }
        for (; position < m_encodedLength; ++position)
        {
            checksum += m_buffer[m_offset + position];
        }
        checksum &= 0xff;

        auto* trailer = m_buffer.data() + m_offset + m_encodedLength;
        trailer[0] = '1';
        trailer[1] = '0';
        trailer[2] = '=';
        utils::writeFixedDigits<3>(checksum, trailer + 3);
        trailer[6] = FieldEnd;
        m_encodedLength += 7;
        return m_encodedLength;
    }

    /**
     * @return the total number of bytes written by the last wrap()/encode(),
     *         including header and trailer
     */
    [[nodiscard]] uint32_t encodedLength() const
    {
        return m_encodedLength;
    }
};

}

#endif //SIMD_FIX_PAYLOAD_ENCODER_HPP