//
// Created by Fredrik Dahlberg on 2026-04-25.
//
// Internal tokenizer and grammar types: the Field token emitted by the SIMD
// tokenizer, the spans over it, and the Category/Presence/RecordType metadata
// enums used by the decoders and the code generator. These are implementation
// detail — consumers should depend on Types.hpp and the generated message
// classes, not on these types.
//

#ifndef SIMD_FIX_DETAIL_TOKENS_HPP
#define SIMD_FIX_DETAIL_TOKENS_HPP

#include <algorithm>
#include <cstdint>
#include <span>
#include <string_view>

#include "org/limitless/fix/Types.hpp"

namespace org::limitless::fix::detail {

struct Field
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
        Data,
        String,
        Timestamp,
        UTCTimeOnly,
        UTCDateOnly,
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
        "Data",
        "String",
        "Timestamp",
        "UTCTimeOnly",
        "UTCDateOnly",
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
        "std::span<const uint8_t>",
        "std::string_view",
        "std::chrono::milliseconds",
        "std::chrono::milliseconds",
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
        if (name.empty())
        {
            return Required;
        }
        constexpr auto end = Strings + std::size(Strings);
        const auto found = std::find(Strings, end, name);
        return found != end ? static_cast<Values>(found - Strings) : Null;
    }
};

using FieldSpan = std::span<Field>;
using TagSpan = std::span<uint16_t>;

struct TokenizedMessage
{
    Buffer data;
    FieldSpan fields;
    TagSpan tags;
    int32_t size;
};

}

#endif //SIMD_FIX_DETAIL_TOKENS_HPP
