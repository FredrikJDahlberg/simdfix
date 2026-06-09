//
// Created by Fredrik Dahlberg on 2026-06-05.
//

#ifndef SIMD_FIX_FIELD_DECODER_HPP
#define SIMD_FIX_FIELD_DECODER_HPP

#include <span>

#include "org/limitless/fix/decoder/DecoderTypes.hpp"
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

    FieldDecoder() = default;

    FieldDecoder(const Buffer data, TokenSpan const tokens, TagSpan const tags, const int32_t size) :
        m_data{data}, m_tokens{tokens}, m_tags{tags}, m_size{size}
    {
    }

    void wrap(const utils::String data,
              const std::span<Token> tokens,
              const std::span<uint16_t> tags,
              const int32_t size)
    {
        m_data = data;
        m_tokens = tokens;
        m_tags = tags;
        m_size = size;
    }

    Token* find(int32_t offset, uint16_t tag)
    {
        // grammar
        while (offset < m_size && m_tokens[offset].m_tag != tag) // FIXME:  && ...
        {
            ++offset;
        }
        return m_tokens.data() + offset;
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
        if (Parent == ParentType::Group)
        {
            std::cout << "PARENT = " << Parent.name() << std::endl;
            // find next delim
        }
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

    // FIXME: access method
    Buffer m_data{};
    TokenSpan m_tokens{};

private:
    TagSpan m_tags{};
    int32_t m_size;

};
}
#endif //SIMD_FIX_FIELD_DECODER_HPP
