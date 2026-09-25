//
// Created by Fredrik Dahlberg on 2026-06-24.
//
// Umbrella header for the hand-written half of simdfix's public API:
//
//   * Version.hpp         — SIMDFIX_VERSION and its parts
//   * Types.hpp           — value/result vocabulary (Result, *Result, Buffer,
//                           SessionContext, FixedString, the Encodable* concepts)
//   * BasicPayloadDecoder — the SIMD tokenizing engine (decode entry point)
//   * PayloadEncoder      — the message-building engine (encode entry point)
//   * basicEncodeResend   — PossDup resend rewriting
//
// The message types come from a spec, via simdfix_generate(). Include the
// generated <namespace-path>/messages/FixMessages.hpp, which pulls in this header
// along with that spec's decoders, encoders, MessageHandler dispatch base, and its
// PayloadDecoder<Protocol> and encodeResend<Protocol> shorthands.
//
// Everything reachable only through "org/limitless/simdfix/detail/..." is internal
// and not part of the supported surface.
//

#ifndef SIMD_FIX_FIX_HPP
#define SIMD_FIX_FIX_HPP

#include "org/limitless/simdfix/Version.hpp"
#include "org/limitless/simdfix/Types.hpp"
#include "org/limitless/simdfix/TokenizedMessage.hpp"
#include "org/limitless/simdfix/decoder/PayloadDecoder.hpp"
#include "org/limitless/simdfix/encoder/PayloadEncoder.hpp"
#include "org/limitless/simdfix/encoder/ResendEncoder.hpp"

#endif //SIMD_FIX_FIX_HPP
