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



enum class ParentType { A, B };

template <typename T>
concept EncodableInteger = std::same_as<T, uint32_t> ||
                           std::same_as<T, uint64_t> ||
                           std::same_as<T, int32_t>;
class FieldEncoder
{
    std::span<uint8_t> m_dest_buffer;
    size_t m_offset = 0;

    template <int32_t Tag, ParentType Parent, typename T>
    static constexpr size_t getEncodedSize() noexcept
    {
        return sizeof(T);
    }

    template <typename T>
    void encode(T value, size_t size)
    {
        if (m_offset + size > m_dest_buffer.size()) [[unlikely]]
        {
            throw std::runtime_error("Buffer overflow");
        }
        // FIXME
        std::span<uint8_t> target = m_dest_buffer.subspan(m_offset, size);
        std::memcpy(target.data(), &value, size);
        m_offset += size;
    }

public:

    /*
    std::optional<uint32_t> empty_field = std::nullopt;
    std::optional<uint32_t> filled_field = 100;

    size_t bytes4 = encoder.setUint32<4, false, ParentType::A>(empty_field);  // Returns 0
    size_t bytes5 = encoder.setUint32<5, false, ParentType::A>(filled_field); // Returns 4
    */

    template <int32_t Tag, bool Required, ParentType Parent, typename T>
    requires EncodableInteger<T>
    size_t setValue(T value)
    {
        constexpr size_t encoded_bytes = getEncodedSize<Tag, Parent, T>();
        writeToBuffer(value, encoded_bytes);
        return encoded_bytes;
    }

    template <int32_t Tag, bool Required, ParentType Parent, typename T>
    requires EncodableInteger<T>
    size_t setValue(std::optional<T> value)
    {
        if (!value.has_value())
        {
            if constexpr (Required)
            {
                throw std::invalid_argument("Required tag cannot be empty");
            }
            return 0;
        }
        return setValue<Tag, Required, Parent>(*value);
    }
};

}

#endif //SIMD_FIX_FIELD_ENCODER_HPP
