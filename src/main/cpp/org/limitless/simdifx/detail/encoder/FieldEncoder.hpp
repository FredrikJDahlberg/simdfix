//
// Created by Fredrik Dahlberg on 2026-06-11.
//

#ifndef SIMD_FIX_FIELD_ENCODER_HPP
#define SIMD_FIX_FIELD_ENCODER_HPP

#include <array>
#include <cassert>
#include <cstddef>
#include <limits>
#include <span>
#include <string_view>

#include "org/limitless/simdifx/Types.hpp"
#include "org/limitless/simdifx/utils/Conversions.hpp"

namespace org::limitless::simdifx::encoder { class DataEncoder; }

namespace org::limitless::simdifx::detail::encoder {

// Encodes individual FIX fields ("TAG=VALUE" followed by SOH) into a buffer.
// The lowest-level building block in the encoder stack; MessageEncoder and
// GroupEncoder both hold a reference to one and delegate field encoding to it.
class FieldEncoder
{
    friend class org::limitless::simdifx::encoder::DataEncoder;

    std::span<uint8_t> m_data{};
    uint32_t m_offset{};
    uint32_t m_encodedLength{};
    // Sentinel that aliases no real day window, so the first encodeTimestamp()
    // always recomputes the date prefix — including for epoch 0 (a window of
    // [-1, MillisPerDay) would have swallowed the whole first day uninitialized).
    int64_t m_cachedDayStartMillis{std::numeric_limits<int64_t>::min()};
    std::array<uint8_t, 9> m_datePrefix{};

    /**
     * Writes "TAG=" for the given tag, leaving the value to be appended by the caller.
     * @tparam Tag tag number to write
     */
    template <FixedString Tag>
    void encode()
    {
        constexpr auto TagPrefix = []
        {
            constexpr auto N = sizeof(Tag) - 1;
            std::array<char, N + 1> buffer{};
            for (std::size_t i = 0; i < N; ++i)
            {
                buffer[i] = Tag.Value[i];
            }
            buffer[N] = '=';
            return buffer;
        }();
        std::memcpy(m_data.data() + m_offset + m_encodedLength, TagPrefix.data(), TagPrefix.size());
        m_encodedLength += TagPrefix.size();
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
        std::memcpy(dst, m_datePrefix.data(), sizeof(m_datePrefix));
        utils::writeTimeOfDay(static_cast<uint32_t>(millis - m_cachedDayStartMillis), dst + sizeof(m_datePrefix));
        m_encodedLength += utils::UTCTimestampLength;
    }

    /**
     * Writes a FIX UTCTimeOnly ("HH:MM:SS.sss", 12 bytes).
     * @param millis milliseconds since midnight
     */
    void encodeUTCTimeOnly(const int64_t millis)
    {
        auto* dst = m_data.data() + m_offset + m_encodedLength;
        utils::writeTimeOfDay(static_cast<uint32_t>(millis), dst);
        m_encodedLength += utils::UTCTimeOnlyLength;
    }

    /**
     * Writes a FIX UTCDateOnly ("YYYYMMDD", 8 bytes).
     * @param millis milliseconds since the Unix epoch
     */
    void encodeUTCDateOnly(const int64_t millis)
    {
        const auto days = millis / utils::MillisPerDay;
        auto* dst = m_data.data() + m_offset + m_encodedLength;
        utils::writeDateOnly(days, dst);
        m_encodedLength += utils::UTCDateOnlyLength;
    }

public:
    FieldEncoder() = default;

    /**
     * Rebinds the encoder to a destination buffer, resetting encodedLength() to 0.
     * @param data destination buffer
     * @param offset byte offset within data at which encoding begins
     */
    void wrap(const std::span<uint8_t> data, const uint32_t offset)
    {
        m_data = data;
        m_offset = offset;
        m_encodedLength = 0;
    }

    /**
     * @return the number of bytes written since the last wrap()
     */
    [[nodiscard]] uint32_t encodedLength() const
    {
        return m_encodedLength;
    }

    /**
     * Writes raw bytes followed by SOH. Used by DataEncoder after writing
     * the tag prefix via encode<Tag>().
     * @param data raw bytes to write
     */
    void writeRaw(const std::span<const uint8_t> data)
    {
        std::memcpy(m_data.data() + m_offset + m_encodedLength, data.data(), data.size());
        m_encodedLength += data.size();
        m_data[m_offset + m_encodedLength] = FieldEnd;
        ++m_encodedLength;
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
     * @tparam ValueType integer type of value
     * @param value value to write
     */
    template <FixedString Tag, typename ValueType>
        requires EncodableInteger<ValueType>
    void encode(const ValueType value)
    {
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
     * @tparam ValueType integer type of value
     * @param value value to write
     */
    template <FixedString Tag, typename ValueType>
        requires EncodableLongInteger<ValueType>
    void encode(const ValueType value)
    {
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
     * @param value value to write
     */
    template <FixedString Tag>
    void encode(const utils::FixedDecimal value)
    {
        encode<Tag>();
        m_encodedLength += utils::fixedDecimalToAscii(value.mantissa(), -8, m_data, m_offset + m_encodedLength);
        m_data[m_offset + m_encodedLength] = FieldEnd;
        ++m_encodedLength;
    }

    /**
     * Writes "TAG=VALUE" followed by SOH, where VALUE is a FIX UTCTimestamp
     * derived from the given duration (see encodeTimestamp()).
     * @tparam Tag tag number to write
     * @tparam DurationType std::chrono::duration type of duration
     * @param duration time since the Unix epoch
     */
    template <FixedString Tag, typename DurationType>
        requires EncodableDuration<DurationType>
    void encode(const DurationType duration)
    {
        encode<Tag>();
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        encodeTimestamp(millis);
        m_data[m_offset + m_encodedLength] = FieldEnd;
        ++m_encodedLength;
    }

    /**
     * Writes "TAG=VALUE" followed by SOH, where VALUE is a FIX UTCTimeOnly
     * ("HH:MM:SS.sss").
     * @tparam Tag tag number to write
     * @param duration time since midnight
     */
    template <FixedString Tag>
    void encodeUTCTimeOnly(const std::chrono::milliseconds duration)
    {
        encode<Tag>();
        encodeUTCTimeOnly(duration.count());
        m_data[m_offset + m_encodedLength] = FieldEnd;
        ++m_encodedLength;
    }

    /**
     * Writes "TAG=VALUE" followed by SOH, where VALUE is a FIX UTCDateOnly
     * ("YYYYMMDD").
     * @tparam Tag tag number to write
     * @param duration time since the Unix epoch
     */
    template <FixedString Tag>
    void encodeUTCDateOnly(const std::chrono::milliseconds duration)
    {
        encode<Tag>();
        encodeUTCDateOnly(duration.count());
        m_data[m_offset + m_encodedLength] = FieldEnd;
        ++m_encodedLength;
    }

    /**
     * Writes "TAG=VALUE" followed by SOH, where VALUE is the FIX string code
     * for the given enum value, obtained from the enum's code() helper. Null
     * values are skipped for optional fields; required fields assert non-null.
     * @tparam Tag tag number to write
     * @tparam Required whether a null value is a programming error
     * @tparam WrapperType scoped enum type whose code() helper is used for the mapping
     * @param value enum value to write
     */
    template <FixedString Tag, bool Required, typename WrapperType>
        requires EncodableEnumWrapper<WrapperType>
    void encode(const WrapperType value)
    {
        if (value == WrapperType::Null)
        {
            if constexpr (Required)
            {
                assert(false && "Required enum field has null value");
            }
            return;
        }
        encode<Tag, std::string_view>(code(value));
    }

    /**
     * Writes "TAG=VALUE" followed by SOH for string-valued fields.
     * @tparam Tag tag number to write
     * @tparam ValueType unused; present for symmetry with the other encode() overloads
     * @param value value to write
     */
    template <FixedString Tag, typename ValueType>
    void encode(const std::string_view value)
    {
        encode<Tag>();
        const auto size = value.size();
        std::memcpy(m_data.data() + m_offset + m_encodedLength, value.data(), size);
        m_encodedLength += size;
        m_data[m_offset + m_encodedLength] = FieldEnd;
        ++m_encodedLength;
    }

    /**
     * Writes "TAG=VALUE" followed by SOH for a nullable 32-bit integer.
     * Skips encoding entirely when the value is null.
     * @tparam Tag tag number to write
     * @tparam NullableType nullable integer type
     * @param value nullable value to write
     */
    template <FixedString Tag, typename NullableType>
        requires EncodableOptionalInteger<NullableType>
    void encode(const NullableType& value)
    {
        if (value.hasValue())
        {
            encode<Tag>(value.value());
        }
    }

    /**
     * Writes "TAG=VALUE" followed by SOH for a nullable 64-bit integer.
     * Skips encoding entirely when the value is null.
     * @tparam Tag tag number to write
     * @tparam NullableType nullable long integer type
     * @param value nullable value to write
     */
    template <FixedString Tag, typename NullableType>
        requires EncodableOptionalLongInteger<NullableType>
    void encode(const NullableType& value)
    {
        if (value.hasValue())
        {
            encode<Tag>(value.value());
        }
    }

    /**
     * Writes "TAG=VALUE" followed by SOH for a nullable string.
     * Skips encoding entirely when the value is null.
     * @tparam Tag tag number to write
     * @tparam NullableType nullable string type
     * @param value nullable value to write
     */
    template <FixedString Tag, typename NullableType>
        requires EncodableOptionalString<NullableType>
    void encode(const NullableType& value)
    {
        if (value.hasValue())
        {
            encode<Tag, std::string_view>(value.value());
        }
    }

    /**
     * Writes "TAG=VALUE" followed by SOH for a nullable FixedDecimal.
     * Skips encoding entirely when the value is null.
     * @tparam Tag tag number to write
     * @tparam NullableType nullable FixedDecimal type
     * @param value nullable value to write
     */
    template <FixedString Tag, typename NullableType>
        requires EncodableOptionalFixedDecimal<NullableType>
    void encode(const NullableType& value)
    {
        if (value.hasValue())
        {
            encode<Tag>(value.value());
        }
    }

    template <FixedString Tag, FixedString Value>
    static uint32_t encode(const uint32_t offset, std::span<uint8_t> buffer)
    {
        constexpr uint32_t TagLength = sizeof(Tag) - 1;
        std::memcpy(buffer.data() + offset, Tag, TagLength);
        uint32_t encodedLength = TagLength;
        buffer[offset + encodedLength] = '=';
        ++encodedLength;
        std::memcpy(buffer.data() + offset + encodedLength, Value, sizeof(Value) - 1);
        encodedLength += sizeof(Value) - 1;
        buffer[offset + encodedLength] = FieldEnd;
        ++encodedLength;
        return encodedLength;
    }

};

}

#endif //SIMD_FIX_FIELD_ENCODER_HPP
