//
// Created by Fredrik Dahlberg on 2026-04-25.
//

#ifndef SIMD_FIX_DICTIONARY_HPP
#define SIMD_FIX_DICTIONARY_HPP

#include <algorithm>
#include <array>

namespace org::limitless::fix::messages {

struct Dictionary {
    uint16_t tag;
    uint16_t type;
    bool mandatory;
    // Add other 64-field metadata here


};

namespace detail {
//
// FIXME: generate
//

inline constexpr std::array<Dictionary, 8> StaticTokenTable{
{
    { 1, 0, false },  // add required and optional
    { 10, 0, true },
    { 49, 12, true },
    { 102, 24, true },
    {627, 10, false },
    {628, 10, false },
    {629, 10, false },
    {630, 10, false }
    // ... total of 64 fields
}
};

}

[[nodiscard]] consteval const Dictionary* dictionary(uint16_t tag) noexcept
{
    const auto it = std::lower_bound(detail::StaticTokenTable.begin(),
                                     detail::StaticTokenTable.end(), tag,
                               [](const Dictionary& lhs, uint16_t rhs)
                               {
                                   return lhs.tag < rhs;
                               });
    if (it != detail::StaticTokenTable.end() && it->tag == tag) {
        return &(*it);
    }
    return nullptr;
}

}

#endif //SIMD_FIX_DICTIONARY_HPP
