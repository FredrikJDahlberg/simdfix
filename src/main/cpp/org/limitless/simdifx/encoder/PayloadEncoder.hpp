//
// Created by Fredrik Dahlberg on 2026-06-11.
//

#ifndef SIMD_FIX_PAYLOAD_ENCODER_HPP
#define SIMD_FIX_PAYLOAD_ENCODER_HPP

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <span>

#include "org/limitless/simdifx/Types.hpp"
#include "org/limitless/simdifx/detail/encoder/FieldEncoder.hpp"
#include "org/limitless/simdifx/detail/simd/Uint8x16.hpp"
#include "org/limitless/simdifx/utils/Conversions.hpp"

namespace org::limitless::simdifx::encoder {

using namespace org::limitless::simdifx::detail::encoder;
using namespace org::limitless::simdifx::detail::simd;

// Writes the FIX header (BeginString, BodyLength, MsgType, SenderCompID, TargetCompID)
// ahead of a message body and, once the body has been encoded, fills in BodyLength
// and appends the trailing CheckSum field.
class PayloadEncoder {
    static constexpr uint32_t BodyLengthDigits = 4;
    // Fixed upper bound on the header prefix ("8="+protocol+SOH, "9=0000"+SOH,
    // "35=?"+SOH, "49="+sender+SOH, "56="+target+SOH). Holding it in an inline
    // array (no heap) lets wrap() copy a compile-time-constant number of bytes,
    // which the compiler inlines instead of calling out to memcpy.
    static constexpr uint32_t MaxPrefixLength = 64;

    std::span<uint8_t> m_buffer{};
    std::array<uint8_t, MaxPrefixLength> m_messagePrefix{};

    uint32_t m_offset{};
    uint32_t m_encodedLength{};

    uint32_t m_bodyLengthOffset;
    uint32_t m_messageTypeOffset;
    uint32_t m_headerLength;
    uint32_t m_headerBodyLength;

public:
    PayloadEncoder() = delete;

    PayloadEncoder(const std::string& beginString, const std::string& sender, const std::string& target)
    {
        // Encode the header fields once into the inline prefix buffer (no heap):
        // "8="+protocol+SOH, "9=0000"+SOH, "35=?"+SOH, "49="+sender+SOH,
        // "56="+target+SOH. wrap() then copies MaxPrefixLength bytes per message.
        assert(beginString.size() + sender.size() + target.size() + 23 <= MaxPrefixLength // FIXME
               && "FIX header prefix exceeds MaxPrefixLength");
        auto buffer = std::span{m_messagePrefix.data(), m_messagePrefix.size()};

        uint32_t offset = 0;
        offset += FieldEncoder::encode("8", beginString.data(), offset, buffer);
        offset += FieldEncoder::encode("9", "0000", offset, buffer);
        const uint32_t bodyLengthStart = offset; // BodyLength counts MsgType onward
        offset += FieldEncoder::encode("35", "?", offset, buffer);
        offset += FieldEncoder::encode("49", sender, offset, buffer);
        offset += FieldEncoder::encode("56", target, offset, buffer);

        m_headerLength = offset;
        m_headerBodyLength = offset - bodyLengthStart;

        m_bodyLengthOffset = beginString.size() + 5; // "8=" + protocol + SOH + "9="
        m_messageTypeOffset = m_bodyLengthOffset + BodyLengthDigits + 4; // SOH + "35="
    }

    /**
     * Rebinds the encoder to a destination buffer and writes the FIX header
     * fields preceding the message body (BeginString, a placeholder BodyLength,
     * a placeholder MsgType, SenderCompID, TargetCompID).
     * @param offset byte offset within buffer at which the header begins
     * @param buffer destination buffer
     * @return *this, for chaining
     */
    PayloadEncoder& wrap(const uint32_t offset, std::span<uint8_t> buffer)
    {
        m_offset = offset;
        m_buffer = buffer;
        // Constant-size copy so the compiler lowers it to inline fixed-width
        // stores rather than an out-of-line memcpy call. Bytes beyond the actual
        // header (m_headerLength) are overwritten by the message body.
        std::memcpy(m_buffer.data() + m_offset, m_messagePrefix.data(), MaxPrefixLength);
        return *this;
    }

    /**
     * @return the offset at which the message body should be encoded
     */
    [[nodiscard]] uint32_t offset() const
    {
        return m_offset + m_headerLength;
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
     * Wraps a message encoder at the body offset and stamps the per-message
     * session header fields that the compile-time header does not carry:
     * MsgSeqNum (tag 34) and SendingTime (tag 52). Both must lead the message
     * body, so this is called before any application fields are encoded. The
     * BeginString, SenderCompID and TargetCompID are already written from the
     * encoder's template parameters by wrap().
     * @tparam MessageEncoderType message encoder type, e.g. LogonEncoder
     * @param message message encoder to wrap
     * @param sequenceNumber MsgSeqNum (tag 34) for this message
     * @param sendingTime SendingTime (tag 52), as time since the Unix epoch
     * @return message, for chaining
     */
    template <typename MessageEncoderType>
    MessageEncoderType& wrapHeader(MessageEncoderType& message,
                                   const uint32_t sequenceNumber,
                                   const std::chrono::milliseconds sendingTime) const
    {
        message.wrap(m_buffer, offset());
        message.sequenceNumber(sequenceNumber);
        message.sendingTime(sendingTime);
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
        const auto bodyLength = m_headerBodyLength + static_cast<uint32_t>(message.encodedLength());
        m_buffer[m_offset + m_messageTypeOffset] = static_cast<uint8_t>(message.type().front());
        utils::writeFixedDigits<BodyLengthDigits>(bodyLength, m_buffer.data() + m_offset + m_bodyLengthOffset);
        m_encodedLength = m_headerLength + static_cast<uint32_t>(message.encodedLength());

        uint32_t position = 0;
        ChecksumAccumulator accum;
        Uint8x16 block;
        for (; position + Uint8x16::Size <= m_encodedLength; position += Uint8x16::Size)
        {
            block.load(m_buffer.data() + m_offset + position);
            accum.add(block);
        }
        uint32_t checksum = accum.value();
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

    static PayloadEncoder build(const std::string& beginString, const std::string& sender, const std::string& target)
    {
        return PayloadEncoder{beginString, sender, target};
    }
};

}

#endif //SIMD_FIX_PAYLOAD_ENCODER_HPP