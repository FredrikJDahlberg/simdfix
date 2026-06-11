//
// Created by Fredrik Dahlberg on 2026-04-25.
//

#ifndef SIMD_FIX_DICTIONARY_HPP
#define SIMD_FIX_DICTIONARY_HPP

#include <algorithm>
#include <string_view>

namespace org::limitless::fix::decoder {

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
        "std::span<const uint8_t>",
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

}


#endif //SIMD_FIX_DICTIONARY_HPP
