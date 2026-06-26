//
// Created by Fredrik Dahlberg on 2026-06-05.
//

#ifndef SIMD_FIX_FIELD_DECODER_HPP
#define SIMD_FIX_FIELD_DECODER_HPP

#include <array>
#include "org/limitless/fix/detail/Expected.hpp"
#include <span>
#include <chrono>
#include <utility>

#include "org/limitless/fix/detail/Tokens.hpp"
#include "org/limitless/fix/detail/simd/LinearSearch.hpp"
#include "org/limitless/fix/utils/Conversions.hpp"

namespace org::limitless::fix::detail::parser {

/**
 * Field-level access to a decoded FIX message: the raw byte buffer, the
 * Field[] array produced by the SIMD tokenizer, and a parallel tag array
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
     * @param fields token array produced by the tokenizer
     * @param tags tag numbers, parallel to fields
     * @param size number of valid fields/tags
     */
    FieldDecoder(const Buffer data, FieldSpan const fields, TagSpan const tags, const int32_t size) :
        m_data{data}, m_fields{fields}, m_tags{tags}, m_size{size}
    {
    }

    /**
     * Rebinds the decoder to a new tokenized message, resetting any group
     * scope from a previous message.
     * @param data raw message bytes
     * @param fields token array produced by the tokenizer
     * @param tags tag numbers, parallel to fields
     * @param size number of valid fields/tags
     */
    void wrap(const Buffer data,
              const std::span<Field> fields,
              const std::span<uint16_t> tags,
              const int32_t size)
    {
        m_data = data;
        m_fields = fields;
        m_tags = tags;
        m_size = size;
        m_scopeDepth = 0;
    }

    /**
     * Linear scan for the first token with the given tag within [offset, end).
     * @param offset starting index
     * @param tag tag number to find
     * @param end exclusive upper bound on the search
     * @return pointer to the matching token, or to m_fields[end] if not found
     */
    [[nodiscard]] Field* find(int32_t offset, const uint16_t tag, const int32_t end)
    {
        while (offset < end && m_tags[offset] != tag)
        {
            ++offset;
        }
        return m_fields.data() + offset;
    }

    /**
     * Linear scan for the first token with the given tag, starting at offset
     * and searching to the end of the message.
     * @param offset starting index
     * @param tag tag number to find
     * @return pointer to the matching token, or to m_fields[m_size] if not found
     */
    [[nodiscard]] Field* find(const int32_t offset, const uint16_t tag)
    {
        return find(offset, tag, m_size);
    }

    /**
     * SIMD search for the first token with the given tag across the whole
     * message.
     * @param tag tag number to find
     * @return pointer to the matching token, or nullptr if not found
     */
    [[nodiscard]] Field* nextField(const uint32_t tag)
    {
        return const_cast<Field*>(std::as_const(*this).nextField(tag));
    }

    /**
     * SIMD search for the first token with the given tag across the whole
     * message.
     * @param tag tag number to find
     * @return pointer to the matching token, or nullptr if not found
     */
    [[nodiscard]] const Field* nextField(const uint32_t tag) const
    {
        const auto index = simd::find(m_tags.data(), m_size, static_cast<uint16_t>(tag));
        return index >= 0 ? &m_fields[index] : nullptr;
    }

    /**
     * Parses the ASCII digits of a token as an unsigned 32-bit integer using
     * SWAR digit parsing. Validates length and digit content.
     * @param token token whose bytes are the digits to convert
     * @return parsed value, or InvalidLength/InvalidValue on bad input
     */
    [[nodiscard]] Uint32Result convertToUint32(const Field* field) const
    {
        if (field->m_length == 0 || field->m_length > utils::MaxUint32Digits)
        {
            return unexpected{Result::InvalidLength};
        }
        const auto* digits = m_data.data() + field->m_position;
        if (!utils::isDigits(digits, field->m_length))
        {
            return unexpected{Result::InvalidValue};
        }
        const auto padded = m_data.size() >= field->m_position + sizeof(uint64_t);
        return static_cast<uint32_t>(utils::asciiToUint64(0, digits, field->m_length, padded));
    }

    /**
     * Parses the ASCII digits of a token, with an optional leading '-', as a
     * signed 32-bit integer. Validates length and digit content.
     * @param token token whose bytes are the digits to convert
     * @return parsed value, or InvalidLength/InvalidValue on bad input
     */
    [[nodiscard]] Int32Result convertToInt32(const Field* field) const
    {
        if (field->m_length == 0 || field->m_length > utils::MaxInt32Chars)
        {
            return unexpected{Result::InvalidLength};
        }
        const auto* digits = m_data.data() + field->m_position;
        const uint32_t start = (digits[0] == '-') ? 1 : 0;
        if (field->m_length - start == 0 || !utils::isDigits(digits + start, field->m_length - start))
        {
            return unexpected{Result::InvalidValue};
        }
        return utils::asciiToInt32(digits, field->m_length);
    }

    /**
     * Parses the ASCII digits of a token, with an optional leading '-' and an
     * optional decimal point, as a FixedDecimal. Validates length and content.
     * @param token token whose bytes are the digits to convert
     * @return parsed value, or InvalidLength/InvalidValue on bad input
     */
    [[nodiscard]] FixedDecimalResult convertToFixedDecimal(const Field* field) const
    {
        if (field->m_length == 0 || field->m_length > utils::MaxDecimalChars)
        {
            return unexpected{Result::InvalidLength};
        }
        const uint8_t* digits = m_data.data() + field->m_position;
        uint32_t start = 0;
        bool negative = false;
        if (digits[0] == '-')
        {
            negative = true;
            start = 1;
        }

        const uint32_t len = field->m_length - start;
        if (len == 0)
        {
            return unexpected{Result::InvalidValue};
        }
        const auto* valueDigits = digits + start;
        uint32_t dotPos = len;
        for (uint32_t i = 0; i < len; ++i)
        {
            if (valueDigits[i] == '.')
            {
                dotPos = i;
                break;
            }
        }
        if (dotPos < len)
        {
            const uint32_t after = len - dotPos - 1;
            if (dotPos == 0 || after == 0 ||
                !utils::isDigits(valueDigits, dotPos) ||
                !utils::isDigits(valueDigits + dotPos + 1, after))
            {
                return unexpected{Result::InvalidValue};
            }
        }
        else if (!utils::isDigits(valueDigits, len))
        {
            return unexpected{Result::InvalidValue};
        }

        const bool padded = m_data.size() >= field->m_position + start + sizeof(uint64_t);
        if (!std::is_constant_evaluated() && padded && len <= 8)
        {
            uint64_t raw;
            std::memcpy(&raw, valueDigits, sizeof(uint64_t));
            int32_t exponent = 0;
            uint32_t digitCount = len;
            if (dotPos < len)
            {
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

        if (!std::is_constant_evaluated() && dotPos < len && dotPos <= 8)
        {
            const uint32_t fracDigits = len - dotPos - 1;
            if (fracDigits <= 8)
            {
                const bool fracPadded = m_data.size() >= field->m_position + start + dotPos + 1 + sizeof(uint64_t);
                const auto intPart = static_cast<int64_t>(utils::asciiToUint64(valueDigits, dotPos, padded));
                const auto fracPart = static_cast<int64_t>(utils::asciiToUint64(valueDigits + dotPos + 1, fracDigits, fracPadded));
                auto mantissa = intPart * static_cast<int64_t>(utils::PowersOf10_64[fracDigits]) + fracPart;
                const auto exponent = -static_cast<int32_t>(fracDigits);
                return utils::FixedDecimal{negative ? -mantissa : mantissa, exponent};
            }
        }

        int64_t mantissa = 0;
        int32_t exponent = 0;
        for (uint32_t i = start; i < field->m_length; ++i)
        {
            const uint8_t ch = digits[i];
            if (ch == '.')
            {
                exponent = -static_cast<int32_t>(field->m_length - i - 1);
                continue;
            }
            mantissa = mantissa * 10 + (ch - '0');
        }
        return utils::FixedDecimal{negative ? -mantissa : mantissa, exponent};
    }

    /**
     * @param index token index
     * @return the token at index
     */
    [[nodiscard]] const Field& fieldAt(const uint32_t index) const
    {
        return m_fields[index];
    }

    /**
     * @param token pointer into m_fields
     * @return the index of token within m_fields
     */
    [[nodiscard]] int32_t indexOf(const Field* field) const
    {
        return static_cast<int32_t>(field - m_fields.data());
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
        if (m_scopeDepth > 0)
        {
            --m_scopeDepth;
        }
    }

    /**
     * SIMD search for Tag, restricted to the current group scope when Parent
     * is ParentType::Group, otherwise across the whole message.
     * @tparam Tag tag number to find
     * @tparam Parent context the tag is being looked up in
     * @return token/tag index, or -1 if not found
     */
    template <int32_t Tag, RecordType Parent>
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
     * Converts the token at a pre-found index to the requested type.
     * @tparam T value type to convert to
     * @param index token index (must be valid)
     * @return converted value
     */
    template <typename T>
    [[nodiscard]] expected<T, Result> valueAt(const int8_t index) const
    {
        const auto& field = m_fields[index];
        if constexpr (std::is_same_v<T, std::string_view>)
        {
            return std::string_view{reinterpret_cast<const char*>(m_data.data() + field.m_position), field.m_length};
        }
        else if constexpr (std::is_same_v<T, std::uint32_t>)
        {
            return convertToUint32(&field);
        }
        else if constexpr (std::is_same_v<T, std::int32_t>)
        {
            return convertToInt32(&field);
        }
        else if constexpr (std::is_same_v<T, utils::FixedDecimal>)
        {
            return convertToFixedDecimal(&field);
        }
        else if constexpr (std::is_same_v<T, std::chrono::milliseconds>)
        {
            const auto ms = cachedTimestamp(m_data.data() + field.m_position, field.m_length);
            if (ms < 0)
            {
                return unexpected{Result::InvalidLength};
            }
            return std::chrono::milliseconds{ms};
        }
        else
        {
            static_assert(sizeof(T) == 0, "unsupported type for valueAt");
        }
    }

    /**
     * Converts the token at a pre-found index to milliseconds since midnight (UTCTimeOnly).
     * @param index token index (must be valid)
     * @return converted value
     */
    [[nodiscard]] constexpr TimestampResult timeOnlyAt(const int8_t index) const
    {
        const auto& field = m_fields[index];
        const auto ms = utils::timeOnlyToMillis(m_data.data() + field.m_position, field.m_length);
        if (ms < 0)
        {
            return unexpected{Result::InvalidLength};
        }
        return std::chrono::milliseconds{ms};
    }

    /**
     * Converts the token at a pre-found index to milliseconds since epoch (UTCDateOnly).
     * @param index token index (must be valid)
     * @return converted value
     */
    [[nodiscard]] constexpr TimestampResult dateOnlyAt(const int8_t index) const
    {
        const auto& field = m_fields[index];
        const auto ms = utils::dateOnlyToEpochUTC(m_data.data() + field.m_position, field.m_length);
        if (ms < 0)
        {
            return unexpected{Result::InvalidLength};
        }
        return std::chrono::milliseconds{ms};
    }

    /**
     * Converts the token at a pre-found index to an enum value.
     * @tparam Enum enum wrapper type
     * @param index token index (must be valid)
     * @return converted enum value
     */
    template <typename Enum>
    [[nodiscard]] constexpr expected<Enum, Result> enumAt(const int8_t index) const
    {
        const auto& field = m_fields[index];
        const auto code = std::string_view{reinterpret_cast<const char*>(m_data.data() + field.m_position), field.m_length};
        const auto value = utils::find<Enum>(code);
        if (value == Enum::Null)
        {
            return unexpected{Result::InvalidValue};
        }
        return value;
    }

    /**
     * Looks up Tag and returns its value as a string view into the message
     * buffer.
     * @tparam Tag tag number to read
     * @tparam Required whether a missing field is an error
     * @tparam Parent context the tag is being looked up in
     * @return field value, or Result::RequiredFieldMissing/Success if absent
     */
    template <int32_t Tag, bool Required, RecordType Parent>
    [[nodiscard]] constexpr StringResult getString() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            const auto& field = m_fields[index];
            if constexpr (Required)
            {
                if (field.m_length == 0)
                {
                    return unexpected{Result::InvalidLength};
                }
            }
            return std::string_view{reinterpret_cast<const char*>(m_data.data() + field.m_position), field.m_length};
        }
        return unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    /**
     * Looks up Tag and parses its value as an unsigned 32-bit integer.
     * @tparam Tag tag number to read
     * @tparam Required whether a missing field is an error
     * @tparam Parent context the tag is being looked up in
     * @return field value, or Result::RequiredFieldMissing/Success if absent
     */
    template <int32_t Tag, bool Required, RecordType Parent>
    [[nodiscard]] Uint32Result getUint32() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            return convertToUint32(&m_fields[index]);
        }
        return unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    /**
     * Looks up Tag and parses its value, with an optional leading '-', as a
     * signed 32-bit integer.
     * @tparam Tag tag number to read
     * @tparam Required whether a missing field is an error
     * @tparam Parent context the tag is being looked up in
     * @return field value, or Result::RequiredFieldMissing/Success if absent
     */
    template <int32_t Tag, bool Required, RecordType Parent>
    [[nodiscard]] Int32Result getInt32() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            return convertToInt32(&m_fields[index]);
        }
        return unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    /**
     * Looks up Tag and parses its value, with an optional leading '-' and an
     * optional decimal point, as a FixedDecimal.
     * @tparam Tag tag number to read
     * @tparam Required whether a missing field is an error
     * @tparam Parent context the tag is being looked up in
     * @return field value, or Result::RequiredFieldMissing/Success if absent
     */
    template <int32_t Tag, bool Required, RecordType Parent>
    [[nodiscard]] FixedDecimalResult getFixedDecimal() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            return convertToFixedDecimal(&m_fields[index]);
        }
        return unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    /**
     * Looks up Tag and parses its value as a FIX UTCTimestamp, returned as
     * milliseconds since the Unix epoch.
     * @tparam Tag tag number to read
     * @tparam Required whether a missing field is an error
     * @tparam Parent context the tag is being looked up in
     * @return field value, or Result::RequiredFieldMissing/Success if absent
     */
    template <uint32_t Tag, bool Required, RecordType Parent>
    [[nodiscard]] constexpr TimestampResult getTimestamp() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            const auto& field = m_fields[index];
            const auto ms = cachedTimestamp(m_data.data() + field.m_position, field.m_length);
            if (ms < 0)
            {
                return unexpected{Result::InvalidLength};
            }
            return std::chrono::milliseconds{ms};
        }
        return unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    /**
     * Looks up Tag and parses its value as a FIX UTCTimeOnly, returned as
     * milliseconds since midnight.
     * @tparam Tag tag number to read
     * @tparam Required whether a missing field is an error
     * @tparam Parent context the tag is being looked up in
     * @return field value, or Result::RequiredFieldMissing/Success if absent
     */
    template <uint32_t Tag, bool Required, RecordType Parent>
    [[nodiscard]] constexpr TimestampResult getUTCTimeOnly() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            const auto& field = m_fields[index];
            const auto ms = utils::timeOnlyToMillis(m_data.data() + field.m_position, field.m_length);
            if (ms < 0)
            {
                return unexpected{Result::InvalidLength};
            }
            return std::chrono::milliseconds{ms};
        }
        return unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    /**
     * Looks up Tag and parses its value as a FIX UTCDateOnly, returned as
     * milliseconds since the Unix epoch at midnight UTC.
     * @tparam Tag tag number to read
     * @tparam Required whether a missing field is an error
     * @tparam Parent context the tag is being looked up in
     * @return field value, or Result::RequiredFieldMissing/Success if absent
     */
    template <uint32_t Tag, bool Required, RecordType Parent>
    [[nodiscard]] constexpr TimestampResult getUTCDateOnly() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            const auto& field = m_fields[index];
            const auto ms = utils::dateOnlyToEpochUTC(m_data.data() + field.m_position, field.m_length);
            if (ms < 0)
            {
                return unexpected{Result::InvalidLength};
            }
            return std::chrono::milliseconds{ms};
        }
        return unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

    /**
     * Looks up Tag and maps its single-character value to an enum type via
     * utils::find.
     * @tparam Tag tag number to read
     * @tparam Required whether a missing field is an error
     * @tparam Enum enum type whose from() helper is used for the mapping
     * @tparam Parent context the tag is being looked up in
     * @return field value, or Result::RequiredFieldMissing/Success if absent
     */
    template <int32_t Tag, bool Required, typename Enum, RecordType Parent>
    [[nodiscard]] constexpr expected<Enum, Result> getEnum() const
    {
        const auto index = findIndex<Tag, Parent>();
        if (index >= 0)
        {
            const auto& field = m_fields[index];
            const auto code = std::string_view{reinterpret_cast<const char*>(m_data.data() + field.m_position), field.m_length};
            const auto value = utils::find<Enum>(code);
            if (value == Enum::Null)
            {
                return unexpected{Result::InvalidValue};
            }
            return value;
        }
        return unexpected{Required ? Result::RequiredFieldMissing : Result::Success};
    }

private:
    /**
     * Parses a UTCTimestamp to milliseconds since the epoch, memoizing the
     * expensive YYYYMMDD->days conversion. Successive timestamps that share a
     * date (e.g. SendingTime and TransactTime, or every message in a day's
     * feed) reuse the cached day count and skip the calendar arithmetic. The
     * cache is keyed on the exact 8 date bytes, so distinct dates never
     * collide, and only validated dates are cached. Persists across wrap() to
     * exploit cross-message date locality.
     * @param data UTCTimestamp bytes
     * @param length 17 or 21
     * @return milliseconds since the epoch, or -1 on bad length/value
     */
    [[nodiscard]] int64_t cachedTimestamp(const uint8_t* data, const uint32_t length) const
    {
        if (length < utils::UTCTimestampShortLength)
        {
            return -1;
        }
        uint64_t word;
        std::memcpy(&word, data, sizeof(word));
        int64_t days;
        if (word == m_dateWord)
        {
            days = m_dateDays;
        }
        else
        {
            days = utils::dateToEpochDays(data);
            if (days < 0)
            {
                return -1;
            }
            m_dateWord = word;
            m_dateDays = days;
        }
        return utils::timestampMillisFromDays(data, length, days);
    }

    Buffer m_data{};
    FieldSpan m_fields{};
    TagSpan m_tags{};
    int32_t m_size{};

    std::array<Scope, MaxGroupDepth> m_scopes{};
    int32_t m_scopeDepth{};

    // Memoized date->days for cachedTimestamp(). m_dateWord holds the last 8
    // date bytes seen; the sentinel never matches 8 ASCII digits.
    mutable uint64_t m_dateWord{~0ull};
    mutable int64_t m_dateDays{0};
};

}

#endif //SIMD_FIX_FIELD_DECODER_HPP

