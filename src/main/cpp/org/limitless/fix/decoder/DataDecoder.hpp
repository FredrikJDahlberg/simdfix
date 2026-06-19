//
// Created by Fredrik Dahlberg on 2026-06-19.
//

#ifndef SIMD_FIX_DATA_DECODER_HPP
#define SIMD_FIX_DATA_DECODER_HPP

#include "org/limitless/fix/decoder/FieldDecoder.hpp"

namespace org::limitless::fix::decoder {

/**
 * Decodes length-prefixed data field pairs (e.g. XmlDataLen/XmlData,
 * RawDataLength/RawData). Uses the length tag's value to determine
 * how many raw bytes to return from the data tag's position, bypassing
 * the normal token length which may be incorrect if the data contains
 * SOH or '=' bytes.
 */
class DataDecoder
{
    FieldDecoder& m_decoder;

public:
    explicit DataDecoder(FieldDecoder& decoder) : m_decoder(decoder)
    {
    }

    DataDecoder(const DataDecoder&) = delete;
    DataDecoder& operator=(const DataDecoder&) = delete;
    DataDecoder(DataDecoder&&) = delete;
    DataDecoder& operator=(DataDecoder&&) = delete;

    /**
     * Looks up the length tag, parses its value, then returns a span of
     * that many raw bytes starting at the data tag's value position.
     * @tparam LengthTag tag number of the length field (e.g. 212)
     * @tparam DataTag tag number of the data field (e.g. 213)
     * @tparam Required whether missing fields are an error
     * @tparam Parent record type context for tag lookup
     * @return raw byte span, or error if either tag is missing
     */
    template <int32_t LengthTag, int32_t DataTag, bool Required, RecordType::Values Parent>
    [[nodiscard]] DataResult get() const
    {
        const auto lengthIndex = m_decoder.findIndex<LengthTag, Parent>();
        if (lengthIndex < 0)
        {
            return std::unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
        }
        const auto length = m_decoder.convertToUint32(&m_decoder.tokenAt(lengthIndex));

        const auto dataIndex = m_decoder.findIndex<DataTag, Parent>();
        if (dataIndex < 0)
        {
            return std::unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
        }
        const auto& token = m_decoder.tokenAt(dataIndex);
        return m_decoder.bufferAt(token.m_position, length);
    }
};

}

#endif //SIMD_FIX_DATA_DECODER_HPP
