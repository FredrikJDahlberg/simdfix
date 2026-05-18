//
// Created by Fredrik Dahlberg on 2026-04-25.
//

#ifndef SIMD_FIX_DICTIONARY_HPP
#define SIMD_FIX_DICTIONARY_HPP

#include <algorithm>

namespace org::limitless::fix::decoder {

struct Dictionary
{
    uint16_t tag;
    uint16_t type;
    bool mandatory;
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
