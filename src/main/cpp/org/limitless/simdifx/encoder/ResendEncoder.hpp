//
// Created by Fredrik Dahlberg on 2026-08-01.
//

#ifndef SIMD_FIX_RESEND_ENCODER_HPP
#define SIMD_FIX_RESEND_ENCODER_HPP

#include <chrono>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include "org/limitless/simdifx/Types.hpp"
#include "org/limitless/simdifx/decoder/PayloadDecoder.hpp"
#include "org/limitless/simdifx/detail/encoder/FieldEncoder.hpp"
#include "org/limitless/simdifx/generated/messages/FixTypes.hpp"
#include "org/limitless/simdifx/utils/Conversions.hpp"

namespace org::limitless::simdifx::encoder
{

/**
 * Rewrites a complete, previously-encoded FIX message for PossDup resend: stamps
 * SendingTime(52) with currentSendingTime — this retransmission's own time, sets
 * PossDupFlag(43)=Y, preserves the message's original SendingTime as OrigSendingTime(122)
 * and recomputes BodyLength(9) and CheckSum(10).
 * @tparam Protocol FIX protocol version (BeginString), matching PayloadDecoder<Protocol>
 * @tparam DataFields optional binary-safe data-field schema, matching
 *         PayloadDecoder<Protocol, DataFields>; defaults to none
 * @param original a complete FIX message, as produced by PayloadEncoder for this same Protocol
 * @param out destination buffer; must hold at least original.size() + 64 bytes
 * @param currentSendingTime this retransmission's own SendingTime
 * @return the patched message length, or 0 if `original` doesn't decode as well-formed FIX,
 *         or `out` is too small
 */
template <generated::messages::Protocol Protocol, typename DataFields = decoder::NoDataFields>
[[nodiscard]] std::size_t encodeResend(const Buffer original, const std::span<uint8_t> out,
                                        const std::chrono::milliseconds currentSendingTime)
{
    // ~55 bytes for the inserted PossDupFlag/OrigSendingTime fields (SendingTime's own field
    // stays the same width — a fixed-length timestamp, just a new value), plus slack for
    // BodyLength gaining a digit.
    if (out.size() < original.size() + 64)
    {
        return 0;
    }

    decoder::PayloadDecoder<Protocol, DataFields> resendDecoder;
    if (resendDecoder.parse(original).m_status != Result::Success)
    {
        return 0;
    }
    const auto fields = resendDecoder.fields();
    constexpr std::size_t MsgSeqNumPosition = 5;
    constexpr std::size_t SendingTimePosition = 6;
    if (fields.size() <= SendingTimePosition)
    {
        return 0;
    }
    const auto& beginString = fields[BeginStringPosition];
    const auto& bodyLength = fields[BodyLengthPosition];
    const auto& msgSeqNum = fields[MsgSeqNumPosition];
    const auto& sendingTime = fields[SendingTimePosition];
    const auto& checksum = fields.back();  // parse() succeeding guarantees the last field is CheckSum(10)

    const std::size_t prefixEnd = beginString.m_position + beginString.m_length + 1;   // right after "8=...\x01"
    const std::size_t bodyStart = bodyLength.m_position + bodyLength.m_length + 1;     // MsgType(35) label
    const std::size_t msgSeqNumEnd = msgSeqNum.m_position + msgSeqNum.m_length + 1;    // right after MsgSeqNum
    const std::size_t origSendingTimeEnd = sendingTime.m_position + sendingTime.m_length + 1;  // right after it
    const std::size_t checksumStart = checksum.m_position - 3;                         // "10=" label

    // "35=...49=...56=...34=<num>\x01" — up to, but excluding, the original SendingTime: that
    // field is replaced outright rather than copied through (see below).
    const std::span<const uint8_t> bodyPrefix(original.data() + bodyStart, msgSeqNumEnd - bodyStart);
    const std::span<const uint8_t> bodySuffix(original.data() + origSendingTimeEnd,
                                               checksumStart - origSendingTimeEnd);
    const std::string_view origSendingTimeValue(
        reinterpret_cast<const char*>(original.data()) + sendingTime.m_position, sendingTime.m_length);

    constexpr std::size_t FreshSendingTimeFieldLength = 3 /* "52=" */ + utils::UTCTimestampLength + 1 /* SOH */;
    constexpr std::string_view PossDupField = "43=Y";
    constexpr std::string_view OrigSendingTimeTag = "122=";
    const auto newBodyLength = static_cast<uint32_t>(bodyPrefix.size() + FreshSendingTimeFieldLength +
                                                       PossDupField.size() + 1 + OrigSendingTimeTag.size() + 1 +
                                                       origSendingTimeValue.size() + bodySuffix.size());

    std::size_t w = 0;
    std::memcpy(out.data(), original.data(), prefixEnd);  // "8=...\x01" verbatim
    w = prefixEnd;

    detail::encoder::FieldEncoder fieldEncoder;
    fieldEncoder.wrap(out, static_cast<uint32_t>(w));
    fieldEncoder.encodeField(BodyLengthTag, newBodyLength);
    w += fieldEncoder.encodedLength();

    std::memcpy(out.data() + w, bodyPrefix.data(), bodyPrefix.size());  // "35=...34=<num>\x01" verbatim
    w += bodyPrefix.size();

    fieldEncoder.wrap(out, static_cast<uint32_t>(w));
    fieldEncoder.encode<"52", std::chrono::milliseconds>(currentSendingTime);  // this retransmission's own time
    w += fieldEncoder.encodedLength();

    std::memcpy(out.data() + w, PossDupField.data(), PossDupField.size());
    w += PossDupField.size();
    out[w++] = FieldEnd;

    std::memcpy(out.data() + w, OrigSendingTimeTag.data(), OrigSendingTimeTag.size());
    w += OrigSendingTimeTag.size();
    std::memcpy(out.data() + w, origSendingTimeValue.data(), origSendingTimeValue.size());
    w += origSendingTimeValue.size();
    out[w++] = FieldEnd;

    std::memcpy(out.data() + w, bodySuffix.data(), bodySuffix.size());  // unprocessed remaining body, verbatim
    w += bodySuffix.size();

    uint32_t sum = 0;
    for (std::size_t i = 0; i < w; ++i)
    {
        sum += out[i];
    }
    out[w++] = '1';
    out[w++] = '0';
    out[w++] = '=';
    utils::writeFixedDigits<3>(sum % 256, out.data() + w);
    w += 3;
    out[w++] = FieldEnd;

    return w;
}

}  // namespace org::limitless::simdifx::encoder

#endif  // SIMD_FIX_RESEND_ENCODER_HPP
