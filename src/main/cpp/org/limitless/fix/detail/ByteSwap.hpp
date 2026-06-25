//
// C++20 polyfill for std::byteswap (C++23).
//

#ifndef SIMD_FIX_BYTESWAP_HPP
#define SIMD_FIX_BYTESWAP_HPP

#include <bit>
#include <cstdint>
#include <type_traits>
#include <version>

namespace org::limitless::fix::detail
{

#if defined(__cpp_lib_byteswap) && __cpp_lib_byteswap >= 202110L

using std::byteswap;

#else

template <typename T>
    requires std::is_integral_v<T>
[[nodiscard]] constexpr T byteswap(T value) noexcept
{
    if constexpr (sizeof(T) == 1)
    {
        return value;
    }
    else if constexpr (sizeof(T) == 2)
    {
        return static_cast<T>(__builtin_bswap16(static_cast<uint16_t>(value)));
    }
    else if constexpr (sizeof(T) == 4)
    {
        return static_cast<T>(__builtin_bswap32(static_cast<uint32_t>(value)));
    }
    else if constexpr (sizeof(T) == 8)
    {
        return static_cast<T>(__builtin_bswap64(static_cast<uint64_t>(value)));
    }
}

#endif

} // namespace org::limitless::fix::detail

#endif //SIMD_FIX_BYTESWAP_HPP
