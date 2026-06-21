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
    DataResult m_result{std::unexpected{Result::Success}};

public:
    explicit DataDecoder(FieldDecoder& decoder) : m_decoder(decoder)
    {
    }

    DataDecoder(const DataDecoder&) = delete;
    DataDecoder& operator=(const DataDecoder&) = delete;
    DataDecoder(DataDecoder&&) = delete;
    DataDecoder& operator=(DataDecoder&&) = delete;

    /**
     * Locates the length and data tag pair and caches the raw byte span.
     * @tparam LengthTag tag number of the length field (e.g. 212)
     * @tparam DataTag tag number of the data field (e.g. 213)
     * @return this decoder
     */
    template <uint32_t LengthTag, uint32_t DataTag>
    DataDecoder& wrap()
    {
        const auto* lengthField = m_decoder.nextField(LengthTag);
        if (lengthField == nullptr)
        {
            m_result = std::unexpected{Result::Success};
            return *this;
        }
        const auto length = m_decoder.convertToUint32(lengthField);
        if (!length.has_value())
        {
            m_result = std::unexpected{length.error()};
            return *this;
        }

        const auto* dataField = m_decoder.nextField(DataTag);
        if (dataField == nullptr)
        {
            m_result = std::unexpected{Result::Success};
            return *this;
        }
        m_result = m_decoder.bufferAt(dataField->m_position, length.value());
        return *this;
    }

    /**
     * @return the raw byte span from the most recent wrap(), or an error
     *         if either tag was missing
     */
    [[nodiscard]] DataResult get() const
    {
        return m_result;
    }
};

}

#endif //SIMD_FIX_DATA_DECODER_HPP
