//
// Created by Fredrik Dahlberg on 2026-06-11.
//

#ifndef SIMD_FIX_FIELD_ENCODER_HPP
#define SIMD_FIX_FIELD_ENCODER_HPP

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <concepts>

#include "org/limitless/fix/CodecTypes.hpp"
#include "org/limitless/fix/utils/Utils.hpp"

namespace org::limitless::fix::encoder {

// Encodes individual FIX fields ("TAG=VALUE" followed by SOH) into a buffer.
// The lowest-level building block in the encoder stack; MessageEncoder and
// GroupEncoder both hold a reference to one and delegate field encoding to it.
class FieldEncoder
{
    std::span<uint8_t> m_data{};
    size_t m_offset{};
    size_t m_encodedLength{};
    int64_t m_cachedDayStartMillis{-1};
    std::array<uint8_t, 9> m_datePrefix{};

    /**
     * Writes "TAG=" for the given tag, leaving the value to be appended by the caller.
     * @tparam Tag tag number to write
     */
    template <FixedString Tag>
    void encode()
    {
        const auto size = sizeof(Tag.value) - 1; // exclude the string literal's null terminator
        std::memcpy(m_data.data() + m_offset + m_encodedLength, Tag.value, size);
        m_encodedLength += size;
        m_data[m_offset + m_encodedLength] = '=';
        ++m_encodedLength;
    }

    /**
     * Writes a FIX UTCTimestamp ("YYYYMMDD-HH:MM:SS.sss", 21 bytes). The "YYYYMMDD-" prefix
     * is cached and only recomputed when millis falls outside the cached day.
     * @param millis milliseconds since the Unix epoch
     */
    void encodeTimestamp(const int64_t millis)
    {
        if (millis < m_cachedDayStartMillis || millis >= m_cachedDayStartMillis + utils::MillisPerDay)
        {
            const auto days = millis / utils::MillisPerDay;
            m_cachedDayStartMillis = days * utils::MillisPerDay;
            utils::writeDatePrefix(days, m_datePrefix.data());
        }
        auto* dst = m_data.data() + m_offset + m_encodedLength;
        std::memcpy(dst, m_datePrefix.data(), m_datePrefix.size());
        utils::writeTimeOfDay(static_cast<uint32_t>(millis - m_cachedDayStartMillis), dst + m_datePrefix.size());
        m_encodedLength += 21;
    }

public:
    FieldEncoder() = default;

    /**
     * Rebinds the encoder to a destination buffer, resetting encodedLength() to 0.
     * @param data destination buffer
     * @param offset byte offset within data at which encoding begins
     */
    void wrap(const std::span<uint8_t> data, const size_t offset)
    {
        m_data = data;
        m_offset = offset;
    }

    /**
     * @return the number of bytes written since the last wrap()
     */
    [[nodiscard]] size_t encodedLength() const
    {
        return m_encodedLength;
    }

    /**
     * Writes "TAG=VALUE" followed by SOH, e.g. for a repeating group's counter field.
     * @param tag tag number to write
     * @param value value to write
     */
    void encodeField(const uint32_t tag, const uint32_t value)
    {
        m_encodedLength += utils::uint32ToAscii(tag, m_data, m_offset + m_encodedLength);
        m_data[m_offset + m_encodedLength] = '=';
        ++m_encodedLength;
        m_encodedLength += utils::uint32ToAscii(value, m_data, m_offset + m_encodedLength);
        m_data[m_offset + m_encodedLength] = FieldEnd;
        ++m_encodedLength;
    }

    /**
     * Writes "TAG=VALUE" followed by SOH for 32-bit (or smaller) signed/unsigned integers.
     * @tparam Tag tag number to write
     * @tparam Required whether a null value must be encoded specially
     * @tparam ValueType integer type of value
     * @param value value to write
     */
    template <FixedString Tag, bool Required, typename ValueType>
    requires EncodableInteger<ValueType>
    void encode(const ValueType value)
    {
        if constexpr (Required)
        {
            // FIXME handle null value
        }
        encode<Tag>();
        if constexpr (std::is_signed_v<ValueType>)
        {
            m_encodedLength += utils::int32ToAscii(value, m_data, m_encodedLength + m_offset);
        }
        else
        {
            m_encodedLength += utils::uint32ToAscii(value, m_data, m_encodedLength + m_offset);
        }
        m_data[m_offset + m_encodedLength] = FieldEnd;
        ++m_encodedLength;
    }

    /**
     * Writes "TAG=VALUE" followed by SOH for 64-bit signed/unsigned integers.
     * @tparam Tag tag number to write
     * @tparam Required whether a null value must be encoded specially
     * @tparam ValueType integer type of value
     * @param value value to write
     */
    template <FixedString Tag, bool Required, typename ValueType>
    requires EncodableLongInteger<ValueType>
    void encode(const ValueType value)
    {
        if constexpr (Required)
        {
            // FIXME handle null value
        }
        encode<Tag>();
        if constexpr (std::is_signed_v<ValueType>)
        {
            m_encodedLength += utils::int64ToAscii(value, m_data, m_offset + m_encodedLength);
        }
        else
        {
            m_encodedLength += utils::uint64ToAscii(value, m_data, m_offset + m_encodedLength);
        }
        m_data[m_offset + m_encodedLength] = FieldEnd;
        ++m_encodedLength;
    }

    /**
     * Writes "TAG=VALUE" followed by SOH, where VALUE is the decimal ASCII
     * representation of a FixedDecimal (see utils::fixedDecimalToAscii).
     * @tparam Tag tag number to write
     * @tparam Required whether a null value must be encoded specially
     * @param value value to write
     */
    template <FixedString Tag, bool Required>
    void encode(const utils::FixedDecimal value)
    {
        if constexpr (Required)
        {
            // FIXME handle null value
        }
        encode<Tag>();
        m_encodedLength += utils::fixedDecimalToAscii(value.mantissa(), value.exponent(), m_data, m_offset + m_encodedLength);
        m_data[m_offset + m_encodedLength] = FieldEnd;
        ++m_encodedLength;
    }

    /**
     * Writes "TAG=VALUE" followed by SOH, where VALUE is a FIX UTCTimestamp
     * derived from the given duration (see encodeTimestamp()).
     * @tparam Tag tag number to write
     * @tparam Required whether a null value must be encoded specially
     * @tparam DurationType std::chrono::duration type of duration
     * @param duration time since the Unix epoch
     */
    template <FixedString Tag, bool Required, typename DurationType>
        requires EncodableDuration<DurationType>
    void encode(const DurationType duration)
    {
        if constexpr (Required)
        {
            // FIXME handle null value
        }
        encode<Tag>();
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        encodeTimestamp(millis);
        m_data[m_offset + m_encodedLength] = FieldEnd;
        ++m_encodedLength;
    }

    /**
     * Writes "TAG=VALUE" followed by SOH, where VALUE is the FIX string code
     * for the given enum value, looked up in WrapperType::Codes.
     * @tparam Tag tag number to write
     * @tparam Required whether a null value must be encoded specially
     * @tparam WrapperType enum wrapper type whose Codes table is used for the mapping
     * @param value enum value to write
     */
    template <FixedString Tag, bool Required, typename WrapperType>
        requires EncodableEnumWrapper<WrapperType>
    void encode(const WrapperType::Values value)
    {
        encode<Tag, Required, std::string_view>(WrapperType::Codes[static_cast<size_t>(value)]);
    }

    /**
     * Writes "TAG=VALUE" followed by SOH for string-valued fields.
     * @tparam Tag tag number to write
     * @tparam Required whether a null value must be encoded specially
     * @tparam ValueType unused; present for symmetry with the other encode() overloads
     * @param value value to write
     */
    template <FixedString Tag, bool Required, typename ValueType>
    void encode(const std::string_view value)
    {
        if constexpr (Required)
        {
            // FIXME
        }
        encode<Tag>();
        const auto size = value.size();
        memcpy(m_data.data() + m_offset + m_encodedLength, value.data(), size);
        m_encodedLength += size;
        m_data[m_offset + m_encodedLength] = FieldEnd;
        ++m_encodedLength;
    }

};

}

#endif //SIMD_FIX_FIELD_ENCODER_HPP
