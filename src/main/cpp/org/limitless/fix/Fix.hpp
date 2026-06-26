//
// Created by Fredrik Dahlberg on 2026-06-24.
//
// Public umbrella header for simdfix. Include this single header to pull in the
// full public API:
//
//   * Types.hpp          — value/result vocabulary (Result, *Result, Buffer,
//                          SessionContext, FixedString, the Encodable* concepts)
//   * PayloadDecoder     — the SIMD tokenizing engine (decode entry point)
//   * PayloadEncoder     — the message-building engine (encode entry point)
//   * Fix message types  — generated decoders, encoders, and the MessageHandler
//                          dispatch base, from protocol.xml
//
// Everything reachable only through "org/limitless/fix/detail/..." is internal
// and not part of the supported surface.
//

#ifndef SIMD_FIX_FIX_HPP
#define SIMD_FIX_FIX_HPP

#include "org/limitless/fix/Types.hpp"
#include "org/limitless/fix/TokenizedMessage.hpp"
#include "org/limitless/fix/decoder/PayloadDecoder.hpp"
#include "org/limitless/fix/encoder/PayloadEncoder.hpp"
#include "org/limitless/fix/generated/messages/FixMessageDecoders.hpp"
#include "org/limitless/fix/generated/messages/FixMessageEncoders.hpp"
#include "org/limitless/fix/generated/messages/FixMessageHandler.hpp"

#endif //SIMD_FIX_FIX_HPP
