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

#include "org/limitless/fix/utils/Utils.hpp"

namespace org::limitless::fix::encoder {

template <typename ValueType>
concept EncodableInteger = std::same_as<ValueType, uint32_t> ||
                           std::same_as<ValueType, int32_t>;

template <typename ValueType>
concept EncodableLongInteger = std::same_as<ValueType, uint64_t> ||
                                std::same_as<ValueType, int64_t>;

template <typename T>
concept EncodableDuration = requires
{
    typename T::rep;
    typename T::period;
} && std::same_as<T, std::chrono::duration<typename T::rep, typename T::period>>;

template <typename T>
concept EncodableEnumWrapper = requires
{
    typename T::Values;
    T::Codes;
} && std::is_enum_v<typename T::Values>;

class FieldEncoder
{
    std::span<uint8_t> m_data{};
    size_t m_offset{};
    size_t m_encodedLength{};
    int64_t m_cachedDayStartMillis{-1};
    std::array<uint8_t, 9> m_datePrefix{};

    template<std::size_t N>
    struct FixedString {
        char value[N];

        constexpr FixedString(const char (&str)[N])
        {
            std::copy_n(str, N, value);
        }
    };

    template <FixedString Tag>
    void encode()
    {
        const auto size = sizeof(Tag.value) - 1; // exclude the string literal's null terminator
        std::memcpy(m_data.data() + m_offset + m_encodedLength, Tag.value, size);
        m_encodedLength += size;
        m_data[m_offset + m_encodedLength] = '=';
        ++m_encodedLength;
    }

    // Writes a FIX UTCTimestamp ("YYYYMMDD-HH:MM:SS.sss", 21 bytes). The "YYYYMMDD-" prefix
    // is cached and only recomputed when millis falls outside the cached day.
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
     * Constructs an encoder over a destination buffer.
     * @param data destination message bytes
     */
    explicit FieldEncoder(const std::span<uint8_t> data) : m_data{data}
    {
    }

    void wrap(const std::span<uint8_t> data, const size_t offset)
    {
        m_data = data;
        m_offset = offset;
        m_encodedLength = 0;
    }

    /**
     * @return the number of bytes written since the last wrap()
     */
    [[nodiscard]] size_t encodedLength() const
    {
        return m_encodedLength;
    }

    template <FixedString Tag, bool Required, typename ValueType>
    requires EncodableInteger<ValueType>
    void encode(const ValueType value)
    {
        if constexpr (Required)
        {
            // check null value
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
        m_data[m_offset + m_encodedLength] = 1;
        ++m_encodedLength;
    }

    template <FixedString Tag, bool Required, typename ValueType>
    requires EncodableLongInteger<ValueType>
    void encode(const ValueType value)
    {
        if constexpr (Required)
        {
            // check null value
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
        m_data[m_offset + m_encodedLength] = 1;
        ++m_encodedLength;
    }

    template <FixedString Tag, bool Required, typename DurationType>
        requires EncodableDuration<DurationType>
    void encode(const DurationType duration)
    {
        if constexpr (Required)
        {
            // check null value
        }
        encode<Tag>();
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        encodeTimestamp(millis);
        m_data[m_offset + m_encodedLength] = 1;
        ++m_encodedLength;
    }

    template <FixedString Tag, bool Required, typename WrapperType>
        requires EncodableEnumWrapper<WrapperType>
    void encode(const WrapperType::Values value)
    {
        encode<Tag, Required, std::string_view>(WrapperType::Codes[static_cast<size_t>(value)]);
    }

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
        m_data[m_offset + m_encodedLength] = 1;
        ++m_encodedLength;
    }

};

}

#endif //SIMD_FIX_FIELD_ENCODER_HPP
