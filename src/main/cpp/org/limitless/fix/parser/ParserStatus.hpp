//
// Created by Fredrik Dahlberg on 2026-04-25.
//

#ifndef SIMD_FIX_PARSERSTATUS_HPP
#define SIMD_FIX_PARSERSTATUS_HPP

#include <cstdint>

namespace org::limitless::fix::parser {

enum class ParserStatus : uint8_t
{
    Success,
    MessageFragment,
    InvalidBeginString,
    InvalidCheckSumTag,
    InvalidBodyLengthTag,
    InvalidBodyLength,
    InvalidCheckSum,
    InvalidTargetCompTag,
    InvalidTargetCompId,
    InvalidSenderCompTag,
    InvalidSenderCompId,
    InvalidSequenceNumber,
    InvalidMessageTypeTag,
    InvalidMessageType,
    RequiredFieldMissing,
    InvalidLength,
    NullValue,
};
}

#endif //SIMD_FIX_PARSERSTATUS_HPP
