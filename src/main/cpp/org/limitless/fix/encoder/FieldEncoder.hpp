//
// Created by Fredrik Dahlberg on 2026-06-11.
//

#ifndef SIMD_FIX_FIELD_ENCODER_HPP
#define SIMD_FIX_FIELD_ENCODER_HPP

#include <cstdint>
#include <cstddef>
#include <span>
#include <string_view>
#include <cstring>
#include <concepts>

#include "org/limitless/fix/utils/Utils.hpp"

namespace org::limitless::fix::encoder {

template <typename ValueType>
concept EncodableInteger = std::same_as<ValueType, uint32_t> ||
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
        const auto size = sizeof(Tag);
        std::memcpy(m_data.data() + m_offset, Tag.value, size);
        m_offset += size;
        m_data[m_offset] = '=';
        ++m_offset;
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

    void wrap(const std::span<uint8_t> data)
    {
        m_data = data;
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
        m_offset += utils::uint32ToAscii(value, m_data, m_offset);
        m_data[m_offset] = 1;
        ++m_offset;
    }

    template <FixedString Tag, bool Required, typename DurationType>
        requires EncodableDuration<DurationType>
    void encode(const DurationType duration)
    {
        encode<Tag>();
        auto ticks = duration.count();
        encode<Tag, Required, decltype(ticks)>(ticks);
    }

    template <FixedString Tag, bool Required, typename WrapperType>
        requires EncodableEnumWrapper<WrapperType>
    void encode(const typename WrapperType::Values value)
    {
        using UnderlyingType = std::underlying_type_t<typename WrapperType::Values>;
        encode<Tag, Required, UnderlyingType>(static_cast<UnderlyingType>(value));
    }

    template <FixedString Tag, bool Required, typename ValueType>
    size_t encode(const std::string_view value)
    {
        encode<Tag>();
        memcpy(m_data.data() + m_offset, value.data(), value.size());
        m_data[m_offset] = 1;
        ++m_offset;
        return 0;
    }

};

}

#endif //SIMD_FIX_FIELD_ENCODER_HPP
