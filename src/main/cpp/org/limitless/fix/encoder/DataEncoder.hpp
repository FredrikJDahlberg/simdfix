//
// Created by Fredrik Dahlberg on 2026-06-19.
//

#ifndef SIMD_FIX_DATA_ENCODER_HPP
#define SIMD_FIX_DATA_ENCODER_HPP

#include "org/limitless/fix/encoder/FieldEncoder.hpp"

namespace org::limitless::fix::encoder {

/**
 * Encodes length-prefixed data field pairs (e.g. XmlDataLen/XmlData).
 * Writes the length tag as an integer, then the data tag followed by
 * raw bytes verbatim — no escaping is applied, so the payload may
 * contain SOH, '=', or any binary content.
 */
class DataEncoder
{
    FieldEncoder& m_encoder;

public:
    explicit DataEncoder(FieldEncoder& encoder) : m_encoder(encoder)
    {
    }

    DataEncoder(const DataEncoder&) = delete;
    DataEncoder& operator=(const DataEncoder&) = delete;
    DataEncoder(DataEncoder&&) = delete;
    DataEncoder& operator=(DataEncoder&&) = delete;

    /**
     * Writes "LengthTag=N SOH DataTag=<raw bytes> SOH".
     * @tparam LengthTag tag number of the length field (e.g. "212")
     * @tparam DataTag tag number of the data field (e.g. "213")
     * @param data raw bytes to encode
     */
    template <FixedString LengthTag, FixedString DataTag>
    void encode(const std::span<const uint8_t> data)
    {
        m_encoder.encode<LengthTag, uint32_t>(static_cast<uint32_t>(data.size()));
        m_encoder.encode<DataTag>();
        m_encoder.writeRaw(data);
    }
};

}

#endif //SIMD_FIX_DATA_ENCODER_HPP
