//
// Created by Fredrik Dahlberg on 2026-05-16.
//

#ifndef SIMD_FIX_LINEAR_HPP
#define SIMD_FIX_LINEAR_HPP

#include <bit>

#include "org/limitless/fix/detail/simd/Uint8x16.hpp"

namespace org::limitless::fix::detail::simd {

[[nodiscard]] inline int32_t find(const uint16_t* array, const int32_t cardinality, const uint16_t key)
{
    int32_t i = 0;
    if (cardinality >= 8)
    {
        const Uint8x16 keys{key};
        Uint8x16 values{};
        for (; i <= cardinality - 8; i += 8)
        {
            const auto remaining = (cardinality - i) * sizeof(uint16_t);
            const uint64_t bits = values.put(array + i, remaining).equal(keys).toUint64();
            if (bits != 0)
            {
                return i + (std::countr_zero(bits) >> 3);
            }
        }
    }
    for (; i < cardinality; ++i)
    {
        if (array[i] == key)
        {
            return i;
        }
    }
    return -1;
}

}

#endif //SIMD_FIX_LINEAR_HPP
