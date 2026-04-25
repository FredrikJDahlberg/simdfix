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
    uint16_t type;   // tag, group id
    bool mandatory;
};

template <size_t Size>
[[nodiscard]] static consteval const Dictionary* dictionary(uint16_t tag, const std::array<Dictionary, Size>& grammar) noexcept
{
    const auto it = std::lower_bound(grammar.begin(), grammar.end(), tag,
                                     [](const Dictionary& lhs, uint16_t rhs){ return lhs.tag < rhs; });
    if (it != grammar.end() && it->tag == tag) {
        return &(*it);
    }
    return nullptr;
}

};


#endif //SIMD_FIX_DICTIONARY_HPP
