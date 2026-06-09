//
// Created by Fredrik Dahlberg on 2026-06-05.
//

#ifndef SIMD_FIX_FIELD_DECODER_HPP
#define SIMD_FIX_FIELD_DECODER_HPP

#include <span>

#include "org/limitless/fix/decoder/Dictionary.hpp"
#include "org/limitless/fix/decoder/Token.hpp"
#include "org/limitless/fix/decoder/Result.hpp"
#include "org/limitless/fix/simd/LinearSearch.hpp"
#include "org/limitless/fix/utils/Utils.hpp"

namespace org::limitless::fix::decoder {

struct FieldDecoder
{
    using Buffer = std::span<const uint8_t>;
    using TokenSpan = std::span<Token>;
    using TagSpan = std::span<uint16_t>;
    using Uint8Result = std::expected<uint8_t, Result::Values>;
    using StringResult = std::expected<utils::String, Result::Values>;
    using Int32Result = std::expected<int32_t, Result::Values>;
    using Uint32Result = std::expected<uint32_t, Result::Values>;
    using Int64Result = std::expected<int64_t, Result::Values>;
    using Uint64Result = std::expected<uint64_t, Result::Values>;
    using TimestampResult = Uint64Result;

    Buffer m_data{};
    TokenSpan m_tokens{};
    TagSpan m_tags{};
    int32_t m_size;

    bool m_group;
    mutable int32_t m_offset;

    FieldDecoder() = default;

    FieldDecoder(const Buffer data, TokenSpan const tokens, TagSpan const tags, const int32_t size) :
        m_data{data}, m_tokens{tokens}, m_tags{tags}, m_size{size}, m_group{false}, m_offset{0}
    {
    }

    void wrap(const utils::String data,
              const std::span<Token> tokens,
              const std::span<uint16_t> tags,
              const int32_t size,
              const bool group = false)
    {
        m_data = data;
        m_tokens = tokens;
        m_tags = tags;
        m_size = size;
        m_group = group;
        m_offset = 0;
    }

    [[nodiscard]] Token* nextField(const uint32_t tag)
    {
        return const_cast<Token*>(std::as_const(*this).nextField(tag));
    }

    [[nodiscard]] const Token* nextField(const uint32_t tag) const
    {
        const auto index = simd::find(m_tags.data(), m_size, tag);
        return index >= 0 ? &m_tokens[index] : nullptr;
    }

    [[nodiscard]] uint32_t nextGroup(int32_t offset, const uint16_t delim) const
    {
        while (m_offset < m_size && m_tokens[offset].m_tag != delim)
        {
            ++m_offset;
        }
        return m_offset;
    }

    [[nodiscard]] constexpr uint32_t convertToUint32(const Token* token) const
    {
        return utils::asciiToDecimal(0, m_data.data() + token->m_position, token->m_length);
    }

    template <int32_t Tag, bool Required, ParentType Parent>
    [[nodiscard]] constexpr StringResult getString() const
    {
        const auto index = simd::find(m_tags.data(), m_size, Tag);
        if (index >= 0)
        {
            const auto& token = m_tokens[index];
            return m_data.subspan(token.m_position, token.m_length);
        }
        return std::unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    template <int32_t Tag, bool Required, ParentType Parent>
    [[nodiscard]] constexpr Uint32Result getUint32() const
    {
        const auto index = simd::find(m_tags.data(), m_size, Tag);
        if (index >= 0)
        {
            return convertToUint32(&m_tokens[index]);
        }
        return std::unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    template <uint32_t Tag, bool Required, ParentType Parent>
    [[nodiscard]] constexpr Uint64Result getTimestamp() const
    {

        const auto index = simd::find(m_tags.data(), m_size , Tag);
        if (index >= 0)
        {
            const auto token = m_tokens[index];
            return utils::dateTimeToEpochUTC(m_data.data() + token.m_position, token.m_length);
        }
        return std::unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    template <int32_t Tag, bool Required, typename Enum, ParentType Parent>
    [[nodiscard]] constexpr std::expected<Enum, Result::Values> getEnum() const
    {
        const auto index = simd::find(m_tags.data(), m_size, Tag);
        if (index >= 0)
        {
            const auto token = m_tokens[index];
            const auto code = m_data[token.m_position];
            return utils::find<Enum>(code);
        }
        return std::unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }
};
}
#endif //SIMD_FIX_FIELD_DECODER_HPP
