//
// Created by Fredrik Dahlberg on 2026-04-25.
//
// Public value and result vocabulary for simdfix: protocol constants, the
// FixedString NTTP helper, the encode-dispatch concepts, SessionContext, the
// Result status type, and the std::expected<…, Result::Values> aliases returned
// by the decoders. This is the API-facing half of the former CodecTypes.hpp;
// the tokenizer-internal types (Field, RecordType, …) live in detail/Tokens.hpp.
//

#ifndef SIMD_FIX_TYPES_HPP
#define SIMD_FIX_TYPES_HPP

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <span>
#include <string_view>
#include "org/limitless/simdifx/detail/Expected.hpp"
#include <chrono>
#include <type_traits>

#include "org/limitless/simdifx/utils/FixedDecimal.hpp"

namespace org::limitless::simdifx {
inline constexpr int32_t MaxGroupDepth = 8;

inline constexpr uint32_t CheckSumTag = 10;
inline constexpr uint32_t BodyLengthTag = 9;
inline constexpr uint32_t MessageTypeTag = 35;

inline constexpr uint32_t CheckSumValueLength = 3;  // checksum is always 3 zero-padded decimal digits

inline constexpr uint8_t TagEnd = '=';
inline constexpr uint8_t FieldEnd = 0x01;

inline constexpr uint32_t BeginStringPosition = 0;
inline constexpr uint32_t BodyLengthPosition = 1;
inline constexpr uint32_t MessageTypePosition = 2;

template<std::size_t N>
struct FixedString
{
    static constexpr uint32_t Size = N;
    char Value[N];

    constexpr FixedString(const char (&str)[N])
    {
        std::copy_n(str, N, Value);
    }

    constexpr operator const char*() const { return Value; }
};

template <typename T>
concept Nullable = requires(const T t)
{
    { t.hasValue() } -> std::same_as<bool>;
    { t.value() };
};

template <typename ValueType>
concept EncodableInteger = !Nullable<ValueType> &&
    (std::same_as<ValueType, uint32_t> || std::same_as<ValueType, int32_t>);

template <typename ValueType>
concept EncodableLongInteger = !Nullable<ValueType> &&
    (std::same_as<ValueType, uint64_t> || std::same_as<ValueType, int64_t>);

template <typename T>
concept EncodableDuration = !Nullable<T> && requires
{
    typename T::rep;
    typename T::period;
} && std::same_as<T, std::chrono::duration<typename T::rep, typename T::period>>;

template <typename T>
concept EncodableEnumWrapper = !Nullable<T> && std::is_enum_v<T> && requires(const T value)
{
    { code(value) } -> std::convertible_to<std::string_view>;
};

template <typename T>
concept EncodableOptionalInteger = Nullable<T> &&
    (std::same_as<std::remove_cvref_t<decltype(std::declval<const T>().value())>, uint32_t> ||
     std::same_as<std::remove_cvref_t<decltype(std::declval<const T>().value())>, int32_t>);

template <typename T>
concept EncodableOptionalLongInteger = Nullable<T> &&
    (std::same_as<std::remove_cvref_t<decltype(std::declval<const T>().value())>, uint64_t> ||
     std::same_as<std::remove_cvref_t<decltype(std::declval<const T>().value())>, int64_t>);

template <typename T>
concept EncodableOptionalString = Nullable<T> &&
    std::convertible_to<decltype(std::declval<const T>().value()), std::string_view>;

template <typename T>
concept EncodableOptionalFixedDecimal = Nullable<T> &&
    std::same_as<std::remove_cvref_t<decltype(std::declval<const T>().value())>, utils::FixedDecimal>;

template <typename T>
concept EncodableMessage = requires(T message, std::span<uint8_t> data)
{
    { message.type() } -> std::convertible_to<std::string_view>;
    { message.wrap(data) };
    { message.encodedLength() } -> std::convertible_to<std::size_t>;
};

// Per-session expectations checked by the generated validate() method.
// Non-owning: the spans must remain valid for the lifetime of the session.
struct SessionContext
{
    std::string_view m_protocol;
    std::string_view m_senderCompId{}; // tag 49 of incoming messages, the counterparty's CompID
    std::string_view m_targetCompId{}; // tag 56 of incoming messages, our own CompID

    SessionContext(const std::string_view protocol,
                   const std::string_view senderCompId,
                   const std::string_view targetCompId) :
        m_protocol(protocol), m_senderCompId(senderCompId), m_targetCompId(targetCompId)
    {
    }
};

enum class Result
{
    NullValue,
    Success,
    MessageFragment,
    InvalidBeginString,
    InvalidCheckSumTag,
    InvalidBodyLengthTag,
    InvalidBodyLength,
    InvalidCheckSum,
    InvalidTargetCompTag,
    InvalidTargetCompId,
    InvalidSenderCompTag,
    InvalidSenderCompId,
    InvalidSequenceNumber,
    InvalidMessageTypeTag,
    InvalidMessageType,
    InvalidSendingTime,
    RequiredFieldMissing,
    InvalidLength,
    InvalidValue
};

static constexpr std::string_view name(const Result value)
{
    switch (value)
    {
        case Result::Success: return "Success";
        case Result::MessageFragment: return "MessageFragment";
        case Result::InvalidBeginString: return "InvalidBeginString";
        case Result::InvalidCheckSumTag: return "InvalidCheckSumTag";
        case Result::InvalidBodyLengthTag: return "InvalidBodyLengthTag";
        case Result::InvalidBodyLength: return "InvalidBodyLength";
        case Result::InvalidCheckSum: return "InvalidCheckSum";
        case Result::InvalidTargetCompTag: return "InvalidTargetCompTag";
        case Result::InvalidTargetCompId: return "InvalidTargetCompId";
        case Result::InvalidSenderCompTag: return "InvalidSenderCompTag";
        case Result::InvalidSenderCompId: return "InvalidSenderCompId";
        case Result::InvalidSequenceNumber: return "InvalidSequenceNumber";
        case Result::InvalidMessageTypeTag: return "InvalidMessageTypeTag";
        case Result::InvalidMessageType: return "InvalidMessageType";
        case Result::InvalidSendingTime: return "InvalidSendingTime";
        case Result::RequiredFieldMissing: return "RequiredFieldMissing";
        case Result::InvalidLength: return "InvalidLength";
        case Result::InvalidValue: return "InvalidValue";
        case Result::NullValue: return "NullValue";
        default: return "??";
    }
}

using String = std::string_view;
using Buffer = std::span<const uint8_t>;
struct ParseResult
{
    uint32_t m_processed;
    Result m_value;
};

using Uint8Result = expected<uint8_t, Result>;
using StringResult = expected<String, Result>;
using Int32Result = expected<int32_t, Result>;
using Uint32Result = expected<uint32_t, Result>;
using Int64Result = expected<int64_t, Result>;
using Uint64Result = expected<uint64_t, Result>;
using TimestampResult = expected<std::chrono::milliseconds, Result>;
using FixedDecimalResult = expected<utils::FixedDecimal, Result>;
using DataResult = expected<Buffer, Result>;

template <typename T>
concept DecodableMessage = requires(T decoder)
{
    { T::MessageId } -> std::convertible_to<uint16_t>;
    { decoder.validate() } -> std::same_as<Result>;
};

}

#endif //SIMD_FIX_TYPES_HPP
