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

#include "org/limitless/simdifx/Types.hpp"

namespace org::limitless::simdifx::detail {
struct Field
{
    uint16_t m_position{};
    uint16_t m_tag{};
    uint16_t m_length{};
};

enum class Category
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

static constexpr std::string_view name(const Category category)
{
    switch (category)
    {
        case Category::Uint8:
            return "Uint8";
        case Category::Int32:
            return "Int32";
        case Category::Uint32:
            return "Uint32";
        case Category::Int64:
            return "Int64";
        case Category::Uint64:
            return "Uint64";
        case Category::Decimal:
            return "Decimal";
        case Category::Data:
            return "Data";
        case Category::String:
            return "String";
        case Category::Timestamp:
            return "Timestamp";
        case Category::UTCTimeOnly:
            return "UTCTimeOnly";
        case Category::UTCDateOnly:
            return "UTCDateOnly";
        case Category::Counter:
            return "Counter";
        case Category::Struct:
            return "Struct";
        case Category::Group:
            return "Group";
        case Category::Enum:
            return "Enum";
        case Category::Component:
            return "Component";
        case Category::Message:
            return "Message";
        default:
        case Category::Null:
            return "??";
    }
}

static constexpr std::string_view type(const Category category)
{
    switch (category)
    {
        case Category::Uint8:
            return "std::uint8_t";
        case Category::Int32:
            return "std::int32_t";
        case Category::Uint32:
            return "std::uint32_t";
        case Category::Int64:
            return "std::int64_t";
        case Category::Uint64:
            return "std::uint64_t";
        case Category::Decimal:
            return "utils::FixedDecimal";
        case Category::Data:
            return "std::span<const uint8_t>";
        case Category::String:
            return "std::string_view";
        case Category::Timestamp:
            return "std::chrono::milliseconds";
        case Category::UTCTimeOnly:
            return "std::chrono::milliseconds";
        case Category::UTCDateOnly:
            return "std::chrono::milliseconds";
        case Category::Counter:
            return "std::uint32_t";
        case Category::Struct:
            return "Struct";
        case Category::Group:
            return "Group";
        case Category::Enum:
            return "Enum";
        case Category::Component:
            return "Component";
        case Category::Message:
            return "Message";
        default:
        case Category::Null:
            return "??";
    }
}

enum class RecordType
{
    Null,
    Message,
    Component,
    Group,
    Enum
};

static constexpr std::string_view name(RecordType type)
{
    switch (type)
    {
        case RecordType::Message:
            return "Message";
        case RecordType::Component:
            return "Component";
        case RecordType::Group:
            return "Group";
        case RecordType::Enum:
            return "Enum";
        case RecordType::Null:
        default:
            return "??";
    }
}

enum class Presence
{
    Null,
    Constant,
    Optional,
    Required
};

[[nodiscard]] static constexpr std::string_view name(const Presence presence)
{
    switch (presence)
    {
        case Presence::Constant:
            return "Constant";
        case Presence::Optional:
            return "Optional";
        case Presence::Required:
            return "Required";
        case Presence::Null:
        default:
            return "??";
    }
}

[[nodiscard]] static constexpr Presence parse(const std::string_view name)
{
    constexpr std::string_view Strings[] = { "Null", "constant", "optional", "required" };
    if (name.empty())
    {
        return Presence::Required;
    }
    const auto end = Strings + std::size(Strings);
    const auto found = std::find(Strings, end, name);
    return found != end ? static_cast<Presence>(found - Strings) : Presence::Null;
}

using FieldSpan = std::span<Field>;
using TagSpan = std::span<uint16_t>;

}

#endif //SIMD_FIX_DETAIL_TOKENS_HPP
