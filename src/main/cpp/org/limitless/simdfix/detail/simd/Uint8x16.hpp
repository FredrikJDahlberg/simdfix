//
// Created by Fredrik Dahlberg on 2026-04-11.
//
// Architecture-selecting header: includes the NEON or SSE backend for
// Uint8x16 and ChecksumAccumulator based on the target platform.
//

#ifndef SIMD_FIX_UINT8X16_HPP
#define SIMD_FIX_UINT8X16_HPP

#if defined(__aarch64__) || defined(_M_ARM64)
#include "Uint8x16_neon.inl"
#elif defined(__x86_64__) || defined(_M_X64)
#include "Uint8x16_sse.inl"
#else
#error "Unsupported architecture: simdfix requires ARM NEON or x86 SSE4.1"
#endif

#endif //SIMD_FIX_UINT8X16_HPP
