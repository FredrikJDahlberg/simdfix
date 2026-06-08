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
        Group
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
        "Group"
    };

    static constexpr std::string_view Types[] =
    {
        "null",
        "std::uint8_t",
        "std::int32_t",
        "std::uint32_t",
        "std::int64_t",
        "std::uint64_t",
        "std::span<const uint8_t>",
        "std::int64_t",
        "std::uint32_t",
        "struct"
        "group"
    };

    constexpr Category() : m_value{Null} {}
    constexpr Category(const Values value) : m_value{value} {}

    explicit constexpr Category(const std::string_view name) : m_value{Null}
    {
        for (int i = 0; i < 7; ++i)
        {
            if (Names[i] == name)
            {
                m_value = static_cast<Values>(i);
                return;
            }
        }
    }

    [[nodiscard]] constexpr std::string_view name() const
    {
        return Names[m_value];
    }
    [[nodiscard]] constexpr std::string_view type() const
    {
        return Types[m_value];
    }

    [[nodiscard]] constexpr bool operator==(const Values v) const
    {
        return m_value == v;
    }
    [[nodiscard]] constexpr bool operator!=(const Values v) const
    {
        return m_value != v;
    }

    Values m_value;
};

struct Parent
{
    enum Values { Null, Message, Component, Group };

    static constexpr std::string_view Names[] = {
        "Null", "Message", "Component", "Group"
    };

    constexpr Parent() : m_value{Null} {}
    constexpr Parent(const Values value) : m_value{value} {}
    constexpr Parent(const std::string_view name) : m_value{Null}
    {
        for (int i = 0; i < 7; ++i)
        {
            if (Names[i] == name)
            {
                m_value = static_cast<Values>(i);
                return;
            }
        }
    }

    [[nodiscard]] constexpr std::string_view name() const { return Names[m_value]; }

    constexpr bool operator==(const Values v) const { return m_value == v; }
    constexpr bool operator!=(const Values v) const { return m_value != v; }

    Values m_value;
};

struct Presence
{
    enum Values { Null, Constant, Optional, Required };

    static constexpr std::string_view Strings[] = { "??", "constant", "optional", "required" };
    static constexpr std::string_view Names[] = { "??", "Constant", "Optional", "Required" };

    constexpr Presence() : m_value{Null} {}

    constexpr Presence(const Values value) : m_value{value} {}

    constexpr Presence(const std::string_view name) : m_value{Required}
    {
        for (int i = 1; i < 4; ++i)
        {
            if (Strings[i] == name)
            {
                m_value = static_cast<Values>(i);
                return;
            }
        }
    }

    [[nodiscard]] constexpr std::string_view name() const
    {
        return Names[m_value];
    }

    Values m_value;
};

struct Dictionary
{
    uint16_t tag;
    uint16_t type;
    Presence presence;
};

[[nodiscard]] static consteval const Dictionary* dictionary(const uint16_t tag, std::span<const Dictionary> grammar) noexcept
{
    const auto it = std::lower_bound(grammar.begin(), grammar.end(), tag,
                                     [](const Dictionary& lhs, const uint16_t rhs)
                                     {
                                         return lhs.tag < rhs;
                                     });
    if (it != grammar.end() && it->tag == tag)
    {
        return &*it;
    }
    return nullptr;
}

}


#endif //SIMD_FIX_DICTIONARY_HPP
