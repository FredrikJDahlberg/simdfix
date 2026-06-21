//
// Created by Fredrik Dahlberg on 2026-06-05.
//

#ifndef SIMD_FIX_FIELD_DECODER_HPP
#define SIMD_FIX_FIELD_DECODER_HPP

#include <array>
#include <expected>
#include <span>
#include <chrono>

#include "org/limitless/fix/CodecTypes.hpp"
#include "org/limitless/fix/simd/LinearSearch.hpp"
#include "org/limitless/fix/utils/Utils.hpp"

namespace org::limitless::fix::decoder {

/**
 * Field-level access to a decoded FIX message: the raw byte buffer, the
 * Token[] array produced by the SIMD tokenizer, and a parallel tag array
 * used for fast lookup. Provides tag lookup (linear and SIMD-accelerated),
 * typed field accessors, and repeating-group scope tracking. Non-owning:
 * all spans must remain valid for the lifetime of use.
 */
class FieldDecoder
{
public:
    FieldDecoder() = default;

    /**
     * Constructs a decoder over an already-tokenized message.
     * @param data raw message bytes
     * @param tokens token array produced by the tokenizer
     * @param tags tag numbers, parallel to tokens
     * @param size number of valid tokens/tags
     */
    FieldDecoder(const Buffer data, TokenSpan const tokens, TagSpan const tags, const int32_t size) :
        m_data{data}, m_tokens{tokens}, m_tags{tags}, m_size{size}
    {
    }

    /**
     * Rebinds the decoder to a new tokenized message, resetting any group
     * scope from a previous message.
     * @param data raw message bytes
     * @param tokens token array produced by the tokenizer
     * @param tags tag numbers, parallel to tokens
     * @param size number of valid tokens/tags
     */
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

    /**
     * Linear scan for the first token with the given tag within [offset, end).
     * @param offset starting index
     * @param tag tag number to find
     * @param end exclusive upper bound on the search
     * @return pointer to the matching token, or to m_tokens[end] if not found
     */
    Token* find(int32_t offset, const uint16_t tag, const int32_t end)
    {
        while (offset < end && m_tags[offset] != tag)
        {
            ++offset;
        }
        return m_tokens.data() + offset;
    }

    /**
     * Linear scan for the first token with the given tag, starting at offset
     * and searching to the end of the message.
     * @param offset starting index
     * @param tag tag number to find
     * @return pointer to the matching token, or to m_tokens[m_size] if not found
     */
    Token* find(const int32_t offset, const uint16_t tag)
    {
        return find(offset, tag, m_size);
    }

    /**
     * SIMD search for the first token with the given tag across the whole
     * message.
     * @param tag tag number to find
     * @return pointer to the matching token, or nullptr if not found
     */
    [[nodiscard]] Token* nextField(const uint32_t tag)
    {
        return const_cast<Token*>(std::as_const(*this).nextField(tag));
    }

    /**
     * SIMD search for the first token with the given tag across the whole
     * message.
     * @param tag tag number to find
     * @return pointer to the matching token, or nullptr if not found
     */
    [[nodiscard]] const Token* nextField(const uint32_t tag) const
    {
        const auto index = simd::find(m_tags.data(), m_size, tag);
        return index >= 0 ? &m_tokens[index] : nullptr;
    }

    /**
     * Parses the ASCII digits of a token as an unsigned 32-bit integer using
     * SWAR digit parsing.
     * @param token token whose bytes are the digits to convert
     * @return parsed value
     */
    [[nodiscard]] constexpr uint32_t convertToUint32(const Token* token) const
    {
        const auto padded = m_data.size() >= token->m_position + sizeof(uint64_t);
        return utils::asciiToUint64(0, m_data.data() + token->m_position, token->m_length, padded);
    }

    /**
     * Parses the ASCII digits of a token, with an optional leading '-', as a
     * signed 32-bit integer.
     * @param token token whose bytes are the digits to convert
     * @return parsed value
     */
    [[nodiscard]] constexpr int32_t convertToInt32(const Token* token) const
    {
        return utils::asciiToInt32(m_data.data() + token->m_position, token->m_length);
    }

    /**
     * Parses the ASCII digits of a token, with an optional leading '-' and an
     * optional decimal point, as a FixedDecimal.
     * @param token token whose bytes are the digits to convert
     * @return parsed value
     */
    [[nodiscard]] constexpr utils::FixedDecimal convertToFixedDecimal(const Token* token) const
    {
        const uint8_t* digits = m_data.data() + token->m_position;
        uint32_t start = 0;
        bool negative = false;
        if (token->m_length != 0 && digits[0] == '-')
        {
            negative = true;
            start = 1;
        }

        const uint32_t len = token->m_length - start;
        const bool padded = m_data.size() >= token->m_position + start + sizeof(uint64_t);
        if (!std::is_constant_evaluated() && padded && len <= 8)
        {
            uint64_t raw;
            std::memcpy(&raw, digits + start, sizeof(uint64_t));
            const uint64_t dotMask = utils::findByte('.', raw);
            int32_t exponent = 0;
            uint32_t digitCount = len;
            if (dotMask != 0)
            {
                const uint32_t dotPos = std::countr_zero(dotMask) / 8;
                exponent = -static_cast<int32_t>(len - dotPos - 1);
                const uint64_t lowerMask = (1ULL << (dotPos * 8)) - 1;
                raw = (raw & lowerMask) | ((raw >> 8) & ~lowerMask);
                --digitCount;
            }
            const uint32_t shift = (sizeof(uint64_t) - digitCount) * 8;
            raw = (raw << shift) | (utils::AsciiZeros & ((1ULL << shift) - 1));
            raw -= utils::AsciiZeros;
            raw = raw * 10 + (raw >> 8);
            auto mantissa = static_cast<int64_t>(
                ((raw & utils::SwarMask) * utils::SwarFactor1 +
                 (raw >> 16 & utils::SwarMask) * utils::SwarFactor2) >> 32);
            return utils::FixedDecimal{negative ? -mantissa : mantissa, exponent};
        }

        int64_t mantissa = 0;
        int32_t exponent = 0;
        for (uint32_t i = start; i < token->m_length; ++i)
        {
            const uint8_t ch = digits[i];
            if (ch == '.')
            {
                exponent = -static_cast<int32_t>(token->m_length - i - 1);
                continue;
            }
            mantissa = mantissa * 10 + (ch - '0');
        }
        return utils::FixedDecimal{negative ? -mantissa : mantissa, static_cast<int8_t>(exponent)};
    }

    /**
     * @param index token index
     * @return the token at index
     */
    [[nodiscard]] const Token& tokenAt(const uint32_t index) const
    {
        return m_tokens[index];
    }

    /**
     * @param token pointer into m_tokens
     * @return the index of token within m_tokens
     */
    [[nodiscard]] int32_t indexOf(const Token* token) const
    {
        return static_cast<int32_t>(token - m_tokens.data());
    }


    /**
     * @param position byte offset into the message
     * @return the raw byte at position
     */
    [[nodiscard]] uint8_t byteAt(const int32_t position) const
    {
        return m_data[position];
    }

    /**
     * @param position byte offset into the message
     * @param length number of bytes
     * @return a span over length raw bytes starting at position
     */
    [[nodiscard]] Buffer bufferAt(const uint32_t position, const uint32_t length) const
    {
        return m_data.subspan(position, length);
    }

    /**
     * A [begin, end) range of token/tag indices for one repeating-group
     * entry.
     */
    struct Scope
    {
        int32_t begin;
        int32_t end;
    };

    /**
     * @return the innermost active repeating-group scope, or the whole
     *         message [0, m_size) if no group scope is active
     */
    [[nodiscard]] Scope groupScope() const
    {
        return m_scopeDepth > 0 ? m_scopes[m_scopeDepth - 1] : Scope{0, m_size};
    }

    /**
     * Enters a repeating-group entry, restricting subsequent findIndex calls
     * with ParentType::Group to [begin, end) until popGroupScope is called.
     * @param begin first token/tag index of the group entry
     * @param end exclusive last token/tag index of the group entry
     * @throws std::runtime_error if group nesting exceeds MaxGroupDepth
     */
    void pushGroupScope(const int32_t begin, const int32_t end)
    {
        if (m_scopeDepth >= MaxGroupDepth) [[unlikely]]
        {
            throw std::runtime_error{"Too many group scopes."};
        }
        m_scopes[m_scopeDepth++] = Scope{begin, end};
    }

    /**
     * Leaves the innermost repeating-group scope entered via pushGroupScope.
     */
    void popGroupScope()
    {
        --m_scopeDepth;
    }

    /**
     * SIMD search for Tag, restricted to the current group scope when Parent
     * is ParentType::Group, otherwise across the whole message.
     * @tparam Tag tag number to find
     * @tparam Parent context the tag is being looked up in
     * @return token/tag index, or -1 if not found
     */
    template <int32_t Tag, RecordType::Values Parent>
    [[nodiscard]] int32_t findIndex() const
    {
        if constexpr (Parent == RecordType::Group)
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

    /**
     * Looks up Tag and returns its value as a string view into the message
     * buffer.
     * @tparam Tag tag number to read
     * @tparam Required whether a missing field is an error
     * @tparam Parent context the tag is being looked up in
     * @return field value, or Result::RequiredFieldMissing/Success if absent
     */
    template <int32_t Tag, bool Required, RecordType::Values Parent>
    [[nodiscard]] constexpr StringResult getString() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            const auto& token = m_tokens[index];
            return std::string_view{reinterpret_cast<const char*>(m_data.data() + token.m_position), token.m_length};
        }
        return std::unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    /**
     * Looks up Tag and parses its value as an unsigned 32-bit integer.
     * @tparam Tag tag number to read
     * @tparam Required whether a missing field is an error
     * @tparam Parent context the tag is being looked up in
     * @return field value, or Result::RequiredFieldMissing/Success if absent
     */
    template <int32_t Tag, bool Required, RecordType::Values Parent>
    [[nodiscard]] constexpr Uint32Result getUint32() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            return convertToUint32(&m_tokens[index]);
        }
        return std::unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    /**
     * Looks up Tag and parses its value, with an optional leading '-', as a
     * signed 32-bit integer.
     * @tparam Tag tag number to read
     * @tparam Required whether a missing field is an error
     * @tparam Parent context the tag is being looked up in
     * @return field value, or Result::RequiredFieldMissing/Success if absent
     */
    template <int32_t Tag, bool Required, RecordType::Values Parent>
    [[nodiscard]] constexpr Int32Result getInt32() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            return convertToInt32(&m_tokens[index]);
        }
        return std::unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    /**
     * Looks up Tag and parses its value, with an optional leading '-' and an
     * optional decimal point, as a FixedDecimal.
     * @tparam Tag tag number to read
     * @tparam Required whether a missing field is an error
     * @tparam Parent context the tag is being looked up in
     * @return field value, or Result::RequiredFieldMissing/Success if absent
     */
    template <int32_t Tag, bool Required, RecordType::Values Parent>
    [[nodiscard]] constexpr FixedDecimalResult getFixedDecimal() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            return convertToFixedDecimal(&m_tokens[index]);
        }
        return std::unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    /**
     * Looks up Tag and parses its value as a FIX UTCTimestamp, returned as
     * milliseconds since the Unix epoch.
     * @tparam Tag tag number to read
     * @tparam Required whether a missing field is an error
     * @tparam Parent context the tag is being looked up in
     * @return field value, or Result::RequiredFieldMissing/Success if absent
     */
    template <uint32_t Tag, bool Required, RecordType::Values Parent>
    [[nodiscard]] constexpr TimestampResult getTimestamp() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            const auto& token = m_tokens[index];
            return std::chrono::milliseconds{utils::dateTimeToEpochUTC(m_data.data() + token.m_position, token.m_length)};
        }
        return std::unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    /**
     * Looks up Tag and maps its single-character value to an enum type via
     * utils::find.
     * @tparam Tag tag number to read
     * @tparam Required whether a missing field is an error
     * @tparam Enum enum type whose Codes table is used for the mapping
     * @tparam Parent context the tag is being looked up in
     * @return field value, or Result::RequiredFieldMissing/Success if absent
     */
    template <int32_t Tag, bool Required, typename Enum, RecordType::Values Parent>
    [[nodiscard]] constexpr std::expected<typename Enum::Values, Result::Values> getEnum() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            const auto& token = m_tokens[index];
            const auto code = std::string_view{reinterpret_cast<const char*>(m_data.data() + token.m_position), token.m_length};
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
