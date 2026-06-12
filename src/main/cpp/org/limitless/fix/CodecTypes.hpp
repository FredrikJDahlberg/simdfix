//
// Created by Fredrik Dahlberg on 2026-04-25.
//

#ifndef SIMD_FIX_DICTIONARY_HPP
#define SIMD_FIX_DICTIONARY_HPP

#include <algorithm>
#include <cstdint>
#include <span>
#include <string_view>
#include <expected>
#include <chrono>

namespace org::limitless::fix {

inline constexpr int32_t MaxGroupDepth = 8;

struct Token
{
    uint16_t m_position;
    uint16_t m_tag;
    uint16_t m_length;
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
        "std::string_view",
        "std::chrono::milliseconds",
        "std::uint32_t",
        "Struct",
        "Group",
        "Enum",
        "Component",
        "Message"
    };

    constexpr Category() : m_value{Null} {}
    constexpr Category(const Values value) : m_value{value} {}

    explicit constexpr Category(const std::string_view name) : m_value{Null}
    {
        constexpr auto end = Names + std::size(Names);
        const auto found = std::find(Names, end, name);
        m_value = found != end ? static_cast<Values>(found - Names) : Null;
    }

    [[nodiscard]] constexpr std::string_view name() const
    {
        return Names[m_value];
    }
    [[nodiscard]] constexpr std::string_view type() const
    {
        return Types[m_value];
    }

    [[nodiscard]] constexpr bool operator==(const Values value) const
    {
        return m_value == value;
    }
    [[nodiscard]] constexpr bool operator!=(const Values value) const
    {
        return m_value != value;
    }

    Values m_value;
};

// Per-session expectations checked by MessageDecoder::checkRequired.
// Non-owning: the spans must remain valid for the lifetime of the session.
struct SessionContext
{
    std::span<const uint8_t> m_expectedSenderCompId{}; // tag 49 of incoming messages, the counterparty's CompID
    std::span<const uint8_t> m_expectedTargetCompId{}; // tag 56 of incoming messages, our own CompID
};

struct ParentType
{
    enum Values { Null, Message, Component, Group, Enum };

    static constexpr std::string_view Names[] = {
        "Null", "Message", "Component", "Group", "Enum"
    };

    constexpr ParentType() : m_value{Null} {}
    constexpr ParentType(const Values value) : m_value{value} {}
    constexpr ParentType(const std::string_view name) : m_value{Null}
    {
        constexpr auto end = Names + std::size(Names);
        const auto found = std::find(Names, end, name);
        m_value = found != end ? static_cast<Values>(found - Names) : Null;
    }

    [[nodiscard]] constexpr std::string_view name() const { return Names[m_value]; }
    constexpr bool operator==(const Values value) const { return m_value == value; }
    constexpr bool operator!=(const Values value) const { return m_value != value; }

    Values m_value;
};

struct Presence
{
    enum Values { Null, Constant, Optional, Required };

    static constexpr std::string_view Strings[] = { "NUll", "constant", "optional", "required" };
    static constexpr std::string_view Names[] = { "Null", "Constant", "Optional", "Required" };

    constexpr Presence() : m_value{Null} {}

    constexpr Presence(const Values value) : m_value{value} {}

    explicit constexpr Presence(const std::string_view name) : m_value{Required}
    {
        constexpr auto end = Strings + std::size(Strings);
        const auto found = std::find(Strings, end, name);
        m_value = found != end ? static_cast<Values>(found - Strings) : Null;
    }

    [[nodiscard]] constexpr std::string_view name() const
    {
        return Names[m_value];
    }

    Values m_value;
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
        "NullValue"
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
using TokenSpan = std::span<Token>;
using TagSpan = std::span<uint16_t>;

}


#endif //SIMD_FIX_DICTIONARY_HPP
