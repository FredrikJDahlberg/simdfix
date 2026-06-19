//
// Created by Fredrik Dahlberg on 2026-04-25.
//

#ifndef SIMD_FIX_DICTIONARY_HPP
#define SIMD_FIX_DICTIONARY_HPP

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <span>
#include <string_view>
#include <expected>
#include <chrono>
#include <type_traits>

#include "org/limitless/fix/utils/FixedDecimal.hpp"

namespace org::limitless::fix {

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
concept EncodableEnumWrapper = !Nullable<T> && requires
{
    typename T::Values;
    T::Codes;
} && std::is_enum_v<typename T::Values>;

template <typename T>
concept EncodableNullableInteger = Nullable<T> &&
    (std::same_as<std::remove_cvref_t<decltype(std::declval<const T>().value())>, uint32_t> ||
     std::same_as<std::remove_cvref_t<decltype(std::declval<const T>().value())>, int32_t>);

template <typename T>
concept EncodableNullableLongInteger = Nullable<T> &&
    (std::same_as<std::remove_cvref_t<decltype(std::declval<const T>().value())>, uint64_t> ||
     std::same_as<std::remove_cvref_t<decltype(std::declval<const T>().value())>, int64_t>);

template <typename T>
concept EncodableNullableString = Nullable<T> &&
    std::convertible_to<decltype(std::declval<const T>().value()), std::string_view>;

template <typename T>
concept EncodableNullableFixedDecimal = Nullable<T> &&
    std::same_as<std::remove_cvref_t<decltype(std::declval<const T>().value())>, utils::FixedDecimal>;

// Matches generated message encoders (e.g. LogonEncoder, HeartbeatEncoder)
// from FixMessageEncoders.hpp: a MessageEncoder subclass identified by a
// MsgType code, wrappable over a destination buffer.
template <typename T>
concept EncodableMessage = requires(T message, std::span<uint8_t> data)
{
    { message.type() } -> std::convertible_to<std::string_view>;
    { message.wrap(data) };
    { message.encodedLength() } -> std::convertible_to<std::size_t>;
};

struct Protocol
{
    enum Values
    {
        Null,
        FIXT_1_1,
        FIX_4_3,
        FIX_4_4,
        Max
    };
    static constexpr std::string_view Codes[]  =
    {
        "?",
        "FIXT.1.1",
        "FIX.4.3",
        "FIX.4.4"
    };

    static constexpr auto code(const Values value)
    {
        return value >= Null && value < Max ? Codes[value] : Codes[0];
    }
};

struct Token
{
    uint16_t m_position{};
    uint16_t m_tag{};
    uint16_t m_length{};
};

struct Category
{
    enum Values
    {
        Null,
        Uint8,
        Int32,
        Uint32,
        Int64,
        Uint64,
        Decimal,
        String,
        Timestamp,
        Counter,
        Struct,
        Group,
        Enum,
        Component,
        Message
    };

    static constexpr std::string_view Names[] =
    {
        "Null",
        "Uint8",
        "Int32",
        "Uint32",
        "Int64",
        "Uint64",
        "FixedDecimal",
        "String",
        "Timestamp",
        "Counter",
        "Struct",
        "Group",
        "Enum",
        "Component",
        "Message"
    };

    static constexpr std::string_view Types[] =
    {
        "Null",
        "std::uint8_t",
        "std::int32_t",
        "std::uint32_t",
        "std::int64_t",
        "std::uint64_t",
        "utils::FixedDecimal",
        "std::string_view",
        "std::chrono::milliseconds",
        "std::uint32_t",
        "Struct",
        "Group",
        "Enum",
        "Component",
        "Message"
    };

    [[nodiscard]] static constexpr std::string_view name(const Values value)
    {
        return Names[value];
    }
    [[nodiscard]] static constexpr std::string_view type(const Values value)
    {
        return Types[value];
    }
};

// Per-session expectations checked by MessageDecoder::checkRequired.
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

struct RecordType
{
    enum Values { Null, Message, Component, Group, Enum };

    static constexpr std::string_view Names[] = {
        "Null", "Message", "Component", "Group", "Enum"
    };

    [[nodiscard]] static constexpr std::string_view name(const Values value) { return Names[value]; }
};

struct Presence
{
    enum Values { Null, Constant, Optional, Required };

    static constexpr std::string_view Strings[] = { "NUll", "constant", "optional", "required" };
    static constexpr std::string_view Names[] = { "Null", "Constant", "Optional", "Required" };

    [[nodiscard]] static constexpr std::string_view name(const Values value)
    {
        return Names[value];
    }

    [[nodiscard]] static constexpr Values parse(const std::string_view name)
    {
        constexpr auto end = Strings + std::size(Strings);
        const auto found = std::find(Strings, end, name);
        return found != end ? static_cast<Values>(found - Strings) : Null;
    }
};

struct Result
{
    enum Values
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
        InvalidLength
    };

    uint32_t m_processed;
    Values m_value;

    constexpr Result() : m_value{Success} {}

    explicit constexpr Result(const Values value) : m_value{value} {}

    constexpr Result(const uint32_t processed, const Values value) : m_processed{processed}, m_value{value}
    {
    }

    [[nodiscard]] constexpr std::string_view name() const
    {
        return Names[m_value];
    }

    constexpr bool operator==(const Values value) const
    {
        return m_value == value;
    }

    constexpr bool operator!=(const Values value) const
    {
        return m_value != value;
    }

    static constexpr std::string_view Names[] = {
        "NullValue",
        "Success",
        "MessageFragment",
        "InvalidBeginString",
        "InvalidCheckSumTag",
        "InvalidBodyLengthTag",
        "InvalidBodyLength",
        "InvalidCheckSum",
        "InvalidTargetCompTag",
        "InvalidTargetCompId",
        "InvalidSenderCompTag",
        "InvalidSenderCompId",
        "InvalidSequenceNumber",
        "InvalidMessageTypeTag",
        "InvalidMessageType",
        "InvalidSendingTime",
        "RequiredFieldMissing",
        "InvalidLength"
    };

};

using String = std::string_view;
using Buffer = std::span<const uint8_t>;
using Uint8Result = std::expected<uint8_t, Result::Values>;
using StringResult = std::expected<String, Result::Values>;
using Int32Result = std::expected<int32_t, Result::Values>;
using Uint32Result = std::expected<uint32_t, Result::Values>;
using Int64Result = std::expected<int64_t, Result::Values>;
using Uint64Result = std::expected<uint64_t, Result::Values>;
using TimestampResult = std::expected<std::chrono::milliseconds, Result::Values>;
using FixedDecimalResult = std::expected<utils::FixedDecimal, Result::Values>;
using TokenSpan = std::span<Token>;
using TagSpan = std::span<uint16_t>;

}


#endif //SIMD_FIX_DICTIONARY_HPP
