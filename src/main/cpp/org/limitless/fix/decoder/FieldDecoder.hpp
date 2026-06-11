//
// Created by Fredrik Dahlberg on 2026-06-05.
//

#ifndef SIMD_FIX_FIELD_DECODER_HPP
#define SIMD_FIX_FIELD_DECODER_HPP

#include <array>
#include <expected>
#include <span>
#include <chrono>

#include "org/limitless/fix/DecoderTypes.hpp"
#include "org/limitless/fix/simd/LinearSearch.hpp"
#include "org/limitless/fix/utils/Utils.hpp"

namespace org::limitless::fix::decoder {

struct FieldDecoder
{
    static constexpr int32_t MaxGroupDepth = 8; // FIXME: verify in processor

    FieldDecoder() = default;

    FieldDecoder(const Buffer data, TokenSpan const tokens, TagSpan const tags, const int32_t size) :
        m_data{data}, m_tokens{tokens}, m_tags{tags}, m_size{size}
    {
    }

    void wrap(const Buffer data,
              const std::span<Token> tokens,
              const std::span<uint16_t> tags,
              const int32_t size)
    {
        m_data = data;
        m_tokens = tokens;
        m_tags = tags;
        m_size = size;
    }

    Token* find(int32_t offset, const uint16_t tag, const int32_t end)
    {
        // Scalar scan over the contiguous tag array; group scopes are only a
        // few tokens wide, so this beats the 8-wide NEON search setup cost.
        while (offset < end && m_tags[offset] != tag)
        {
            ++offset;
        }
        return m_tokens.data() + offset;
    }

    Token* find(const int32_t offset, const uint16_t tag)
    {
        return find(offset, tag, m_size);
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
        const auto padded = m_data.size() >= token->m_position + sizeof(uint64_t);
        return utils::asciiToUint64(0, m_data.data() + token->m_position, token->m_length, padded);
    }

    [[nodiscard]] const Token& tokenAt(const uint32_t index) const
    {
        return m_tokens[index];
    }

    [[nodiscard]] int32_t indexOf(const Token* token) const
    {
        return static_cast<int32_t>(token - m_tokens.data());
    }


    [[nodiscard]] uint8_t byteAt(const int32_t position) const
    {
        return m_data[position];
    }

    struct Scope
    {
        int32_t begin;
        int32_t end;
    };

    [[nodiscard]] Scope groupScope() const
    {
        return m_scopeDepth > 0 ? m_scopes[m_scopeDepth - 1] : Scope{0, m_size};
    }

    void pushGroupScope(const int32_t begin, const int32_t end)
    {
        m_scopes[m_scopeDepth++] = Scope{begin, end};
    }

    void popGroupScope()
    {
        --m_scopeDepth;
    }

    template <int32_t Tag, ParentType Parent>
    [[nodiscard]] int32_t findIndex() const
    {
        if constexpr (Parent == ParentType::Group)
        {
            const auto [begin, end] = groupScope();
            const auto index = simd::find(m_tags.data() + begin, end - begin, Tag);
            return index >= 0 ? begin + index : -1;
        }
        else
        {
            return simd::find(m_tags.data(), m_size, Tag);
        }
    }

    template <int32_t Tag, bool Required, ParentType Parent>
    [[nodiscard]] constexpr StringResult getString() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            const auto& token = m_tokens[index];
            auto subspan = m_data.subspan(token.m_position, token.m_length);
            return std::string_view{reinterpret_cast<const char*>(subspan.data()), subspan.size()};
        }
        return std::unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    template <int32_t Tag, bool Required, ParentType Parent>
    [[nodiscard]] constexpr Uint32Result getUint32() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            return convertToUint32(&m_tokens[index]);
        }
        return std::unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    template <uint32_t Tag, bool Required, ParentType Parent>
    [[nodiscard]] constexpr TimestampResult getTimestamp() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            const auto token = m_tokens[index];
            return std::chrono::milliseconds{utils::dateTimeToEpochUTC(m_data.data() + token.m_position, token.m_length)};
        }
        return std::unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    template <int32_t Tag, bool Required, typename Enum, ParentType Parent>
    [[nodiscard]] constexpr std::expected<Enum, Result::Values> getEnum() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            const auto token = m_tokens[index];
            const auto code = m_data[token.m_position];
            return utils::find<Enum>(code);
        }
        return std::unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

private:
    Buffer m_data{};
    TokenSpan m_tokens{};
    TagSpan m_tags{};
    int32_t m_size{};

    std::array<Scope, MaxGroupDepth> m_scopes{};
    int32_t m_scopeDepth{};
};
}
#endif //SIMD_FIX_FIELD_DECODER_HPP
