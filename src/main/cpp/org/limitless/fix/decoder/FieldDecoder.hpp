//
// Created by Fredrik Dahlberg on 2026-06-05.
//

#ifndef SIMD_FIX_FIELD_DECODER_HPP
#define SIMD_FIX_FIELD_DECODER_HPP

#include <span>

#include "org/limitless/fix/decoder/Token.hpp"
#include "org/limitless/fix/decoder/Result.hpp"
#include "org/limitless/fix/simd/LinearSearch.hpp"

namespace org::limitless::fix::decoder {

struct FieldDecoder
{
    using Buffer = std::span<const uint8_t>;
    using TokenSpan = std::span<Token>;
    using TagSpan = std::span<uint16_t>;
    using StringResult = std::expected<utils::String, Result::Values>;
    using Uint32Result = std::expected<uint32_t, Result::Values>;

    Buffer m_data{};
    TokenSpan m_tokens{};
    TagSpan m_tags{};
    int32_t m_size;

    FieldDecoder() = default;

    FieldDecoder(const Buffer data, TokenSpan const tokens, TagSpan const tags, const int32_t size) :
        m_data{data}, m_tokens{tokens}, m_tags{tags}, m_size{size}
    {
    }

    void wrap(const utils::String data, const std::span<Token> tokens, const std::span<uint16_t> tags, const int32_t size)
    {
        m_data = data;
        m_tokens = tokens;
        m_tags = tags;
        m_size = size;
    }

    [[nodiscard]] Token* next(const uint32_t tag)
    {
        const auto index = simd::find(m_tags.data(), m_size, tag);
        return index >= 0 ? &m_tokens[index] : nullptr;
    }

    [[nodiscard]] const Token* next(const uint32_t tag) const
    {
        const auto index = simd::find(m_tags.data(), m_size, tag);
        return index < m_size ? &m_tokens[index] : nullptr;
    }

    constexpr uint32_t convertToUint32(const Token* token) const
    {
        return utils::asciiToDecimal(0, m_data.data() + token->position, token->length);
    }

    template <int32_t Tag, bool Required>
    [[nodiscard]] constexpr StringResult getString() const
    {
        const auto index = simd::find(m_tags.data(), m_size, Tag);
        if (index >= 0)
        {
            const auto& token = m_tokens[index];
            return m_data.subspan(token.position, token.length);
        }
        return std::unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    template <int32_t Tag, bool Required>
    [[nodiscard]] constexpr Uint32Result getUint32() const
    {
        const auto index = simd::find(m_tags.data(), m_size, Tag);
        if (index >= 0)
        {
            return convertToUint32(&m_tokens[index]);
        }
        return std::unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }
};
}
#endif //SIMD_FIX_FIELD_DECODER_HPP
