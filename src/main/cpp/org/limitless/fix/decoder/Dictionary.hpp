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
    enum Values { Null, Int32, String, Message, Component, Group, GroupMember };

    static constexpr std::string_view Names[] = {
        "Null", "Int32", "String", "Message", "Component", "Group", "GroupMember"
    };

    constexpr Category() : m_value{Null} {}

    constexpr Category(const Values value) : m_value{value} {}

    constexpr Category(const std::string_view name) : m_value{Null}
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
