//
// Created by Fredrik Dahlberg on 2026-06-11.
//

#ifndef SIMD_FIX_FIELD_ENCODER_HPP
#define SIMD_FIX_FIELD_ENCODER_HPP

#include "org/limitless/fix/CodecTypes.hpp"

#include <cstdint>
#include <cstddef>
#include <span>
#include <string_view>
#include <stdexcept>
#include <cstring>
#include <optional>
#include <concepts>
#include <stdexcept>

namespace org::limitless::fix::encoder {


template <typename ValueType>
concept EncodableInteger = std::same_as<ValueType, int32_t> ||
                           std::same_as<ValueType, uint32_t> ||
                           std::same_as<ValueType, uint64_t> ||
                           std::same_as<ValueType, int64_t>;
class FieldEncoder
{
    Buffer m_data{};
    size_t m_offset{};

    template <int32_t Tag, ParentType::Values Parent, typename ValueType>
    static constexpr size_t getEncodedSize() noexcept
    {
        return sizeof(ValueType);
    }

    template <typename ValueType>
    void encode(const ValueType value, const size_t size)
    {
        if (m_offset + size > m_data.size()) [[unlikely]]
        {
            throw std::runtime_error("Buffer overflow");
        }
        // FIXME
        // std::span<uint8_t> target = m_data.subspan(m_offset, size);
        // std::memcpy(target.data(), &value, size);
        m_offset += size;
    }

public:

    FieldEncoder() = default;

    /**
     * Constructs an encoder over a destination buffer.
     * @param data destination message bytes
     */
    explicit FieldEncoder(const Buffer data) : m_data{data}
    {
    }

    void wrap(const Buffer data)
    {
        m_data = data;
    }

    template <int32_t Tag, bool Required, ParentType::Values Parent, typename ValueType>
    requires EncodableInteger<ValueType>
    size_t encode(const ValueType value)
    {
        constexpr size_t encoded_bytes = getEncodedSize<Tag, Parent, ValueType>();
        encode(value, encoded_bytes);
        return encoded_bytes;
    }

    template <int32_t Tag, bool Required, ParentType::Values Parent, typename ValueType>
    size_t encode(const ValueType value)
    {
        constexpr size_t encoded_bytes = getEncodedSize<Tag, Parent, ValueType>();
        encode(value, encoded_bytes);
        return encoded_bytes;
    }

    template <int32_t Tag, bool Required, ParentType::Values Parent, typename ValueType>
    size_t encode(const std::optional<ValueType> value)
    {
        if (!value.has_value())
        {
            if constexpr (Required)
            {
                throw std::invalid_argument("Required tag cannot be empty");
            }
            return 0;
        }
        return encode<Tag, Required, Parent>(*value);
    }

};

}

#endif //SIMD_FIX_FIELD_ENCODER_HPP
