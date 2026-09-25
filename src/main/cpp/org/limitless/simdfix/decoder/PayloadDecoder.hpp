//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#ifndef SIMD_FIX_PAYLOAD_DECODER_HPP
#define SIMD_FIX_PAYLOAD_DECODER_HPP

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <span>
#include <type_traits>

#include "org/limitless/simdfix/Types.hpp"
#include "org/limitless/simdfix/decoder/PayloadHandler.hpp"
#include "org/limitless/simdfix/detail/Tokens.hpp"
#include "org/limitless/simdfix/detail/simd/Uint8x16.hpp"
#include "org/limitless/simdfix/utils/Conversions.hpp"

namespace org::limitless::simdfix::decoder {

using namespace org::limitless::simdfix::detail;
using namespace org::limitless::simdfix::detail::simd;

struct NoDataFields
{
    static constexpr int32_t dataTag(const uint16_t)
    {
        return -1;
    }
};

/**
 * SIMD tokenizer for raw FIX message bytes. Scans the buffer 16 bytes at a
 * time with NEON to locate tag/value boundaries ('=' and SOH), filling a
 * fixed-size Token[] array (position + tag + length, no copies) and a
 * parallel tag array. Also validates BeginString, BodyLength, and the
 * trailing CheckSum. Handles messages that are fragmented across chunk
 * boundaries, including tags split across a 16-byte block boundary.
 * Define SIMDFIX_TRACE to print every parsed block and token to stdout.
 *
 * Each generated spec provides a PayloadDecoder<Protocol> alias that fills in
 * MaxFields and DataFields from its config.
 *
 * @tparam Protocol FIX protocol version enumerator of a generated spec (e.g.
 *         Protocol::FIXT_1_1); its wire code, found by code(Protocol) through
 *         argument-dependent lookup, is the BeginString validated on parse()
 * @tparam MaxFields capacity of the token array, i.e. the most fields one message may have
 * @tparam DataFields compile-time schema providing dataTag(uint16_t) for
 *         binary-safe data field skipping; NoDataFields disables the skip
 */
template <auto Protocol, std::size_t MaxFields, typename DataFields = NoDataFields>
    requires std::is_enum_v<decltype(Protocol)>
class BasicPayloadDecoder
{
    static constexpr size_t MaxSize = MaxFields;

    static constexpr uint32_t RequiredFieldCount = 7;
    static constexpr uint32_t MessageFragmentLimit = 32;
    static constexpr uint64_t CheckSumMask = 1 |  '1' << 8 | '0' << 16 | '=' << 24 | 1ULL << 56;

    static constexpr std::string_view ProtocolCode = code(Protocol);
    static constexpr uint32_t ProtocolLength = static_cast<uint32_t>(ProtocolCode.size());
    static constexpr auto ProtocolPrefix = []
    {
        std::array<uint8_t, ProtocolLength + 3> result{};
        result[0] = '8';
        result[1] = '=';
        for (std::size_t i = 0; i < ProtocolLength; ++i)
        {
            result[i + 2] = static_cast<uint8_t>(ProtocolCode[i]);
        }
        result[ProtocolLength + 2] = FieldEnd;
        return result;
    }();

    static inline const Uint8x16 TagEndsBlock{'='};
    static inline const Uint8x16 FieldEndsBlock{0x01};
    static inline const Uint8x16 ZerosBlock{'0'};
    static inline const Uint8x16 NineMask{9};

    std::array<Field, MaxSize> m_fields{};
    std::array<uint16_t, MaxSize> m_tags{};

    Uint8x16 m_block{};
    uint32_t m_tag{};
    int32_t m_position{};
    size_t m_count{};

public:
    using position_t = uint32_t;
    using length_t = uint16_t;
    using value_t = uint16_t;
    using data_t = uint8_t;

    BasicPayloadDecoder() = default;
    ~BasicPayloadDecoder() = default;

    BasicPayloadDecoder(const BasicPayloadDecoder&) = delete;
    BasicPayloadDecoder& operator=(const BasicPayloadDecoder&) = delete;
    BasicPayloadDecoder(BasicPayloadDecoder&&) = delete;
    BasicPayloadDecoder& operator=(BasicPayloadDecoder&&) = delete;

    /**
     * Used for testing.
     * @return the tokens produced by the most recent parse() call
     */
    [[nodiscard]] std::span<Field> fields() noexcept
    {
        return { m_fields.data(), m_count };
    }

    /**
     * Tokenizes buffer and, on success, hands the whole TokenizedMessage to
     * handler. The handler recovers the MsgType with message.messageId() and
     * dispatches accordingly.
     * @param buffer raw FIX message bytes
     * @param handler receives the tokenized message via handle(); see
     *        PayloadHandler
     * @return Result::Success and handler's status, or the tokenizer's
     *         failure status if parsing failed
     */
    template <PayloadHandler Handler>
    ParseResult parse(const Buffer buffer, Handler& handler)
    {
        auto result = parse(buffer);
        if (result.m_status != Result::Success)
        {
            return { result.m_processed, result.m_status };
        }

        const TokenizedMessage message{buffer, {m_fields.data(), m_count}, {m_tags.data(), m_count},
            static_cast<int32_t>(m_count)};
        result.m_status = handler.handle(message);
        return result;
    }

    /**
     * Tokenizes buffer into m_fields/m_tags: validates BeginString, scans
     * 16 bytes at a time to locate tag/value boundaries, and validates
     * BodyLength and CheckSum on completion.
     *
     * Data fields are rare, so the message is first scanned without checking
     * each tag against DataFields, which keeps that check out of the hot loop.
     * The tags are checked once afterwards. If one is a data length tag, the
     * tokens that scan made from bytes inside each data payload are dropped
     * (see resolveDataFields); only if that is not possible does the scan
     * resume from the first data length tag with data payloads skipped.
     * @param buffer raw FIX message bytes
     * @return Result::Success with the number of bytes processed, or a
     *         failure status (e.g. Result::MessageFragment if buffer ends
     *         mid-message)
     */
    ParseResult parse(const Buffer buffer)
    {
        m_count = 0;
        m_tag = 0;
        if (buffer.size() < MessageFragmentLimit)
        {
            return { 0, Result::MessageFragment };
        }
        if (std::memcmp(buffer.data(), ProtocolPrefix.data(), ProtocolPrefix.size()) != 0)
        {   // a corrupt stream should be closed
            return { 0, Result::InvalidBeginString };
        }

        m_fields[BeginStringPosition] = { 2, 8, ProtocolLength };
        m_tags[BeginStringPosition] = 8;
        m_count = 1;

        const auto result = tokenize<false>(buffer, 0, 0, 4);
        if constexpr (!std::is_same_v<DataFields, NoDataFields>)
        {
            if (hasDataLengthTag()) [[unlikely]]
            {
                return resolveDataFields(buffer, result);
            }
        }
        return result;
    }

private:

    /**
     * Scans full 16-byte blocks from offset, tokenizing each via processBlock,
     * then the partial block after them, and validates the message.
     * @tparam SkipData whether to skip data payloads as their length tags close
     * @param buffer raw FIX message bytes
     * @param offset block offset to start at
     * @param blockSum byte sum of everything before offset
     * @param bits bit position to resume the first block's scan from
     * @return as for parse()
     */
    template <bool SkipData>
    ParseResult tokenize(const Buffer buffer, position_t offset, uint64_t blockSum, position_t bits)
    {
        const auto data = buffer.data();
        const auto length = static_cast<length_t>(buffer.size());
        // Padded so binaryToDecimal can always read 8 bytes from any digits[0..15] start.
        data_t digits[Uint8x16::Size + sizeof(uint64_t)]{};
        bool complete = false;
        for (; offset + 15 < length && !complete; offset += Uint8x16::Size)
        {
            m_block.load(data + offset);
            blockSum += m_block.sum();
#if defined(SIMDFIX_TRACE)
            utils::print(16, data + offset);
#endif
            const Uint8x16 shifted{m_block - ZerosBlock};
            const Uint8x16 digitFlags{shifted <= NineMask};
            const Uint8x16 tagEnds{m_block == TagEndsBlock};
            const Uint8x16 fieldEnds{m_block == FieldEndsBlock};

            // A digit is valid when followed by '=' or a digit. Propagates up to
            // 4 positions via Kogge-Stone doubling (shift by 1, then by 2) instead
            // of four sequential shift-by-1 steps, shortening the dependency chain.
            Uint8x16 after{digitFlags & tagEnds.shiftLeft<1>()};
            Uint8x16 afterReach{digitFlags};
            after |= afterReach & after.shiftLeft<1>();
            afterReach &= afterReach.shiftLeft<1>();
            after |= afterReach & after.shiftLeft<2>();

            // A digit is valid if preceded by 0x1 or a digit; same doubling scheme.
            Uint8x16 before{digitFlags & fieldEnds.shiftRight<1>()};
            Uint8x16 beforeReach{digitFlags};
            before |= beforeReach & before.shiftRight<1>();
            beforeReach &= beforeReach.shiftRight<1>();
            before |= beforeReach & before.shiftRight<2>();

            const Uint8x16 validTags{after | before};
            const Uint8x16 tagsBlock{validTags.whenTrue(shifted)};
            auto tagDigits = validTags.toUint64();  // 16 bytes to 4-bit nibble
            tagsBlock.get(0, digits);
            tagDigits >>= bits;
#if defined(SIMDFIX_TRACE)
            for (const auto digit : digits)
            {
                std::printf("%02x ", digit);
            }
            std::printf("\n");
#endif
            complete = processBlock<SkipData>(tagDigits, digits, bits, data, length, offset, blockSum);
            bits = 0;
        }
        if (complete)
        {
            m_fields[m_count - 1].m_length = 3; // previous field is checksum
        }
        else if (offset < buffer.size())
        {
            processTrailer(offset, buffer);
        }
        auto result = checkRequiredFields(data, blockSum, offset, length);
        if (result.m_status != Result::Success)
        {
            m_fields[m_count - 1].m_tag = 0;
        }
        return result;
    }
    /**
     * Corrects a scan that ran without data skipping for the data fields in the
     * message. Tag detection is local (digits after SOH and before '='), so that
     * scan tokenized everything outside the data payloads correctly, and its
     * BodyLength and CheckSum checks do not depend on the tokens between them.
     * Only the tokens made from bytes inside a payload are wrong: they are
     * dropped, and each data field's length is set from its length field. If a
     * payload holds something that ended the scan early, such as "10=", the
     * message is rescanned from the first data field instead.
     * @param buffer raw FIX message bytes
     * @param first the result of the scan without data skipping
     * @return as for parse()
     */
    [[gnu::noinline]] ParseResult resolveDataFields(const Buffer buffer, const ParseResult first)
    {
        if (first.m_status == Result::Success && dropDataPayloadTokens(buffer))
        {
            return first;
        }
        return rescanFromDataField(buffer, first);
    }

    /**
     * Drops the tokens made from data payload bytes, as described for
     * resolveDataFields. Leaves the tokens unchanged where it returns false.
     * @param buffer raw FIX message bytes, successfully scanned
     * @return false if a data field is not laid out as expected, or its payload
     *         runs into the CheckSum field, so the message must be rescanned
     */
    [[nodiscard]] bool dropDataPayloadTokens(const Buffer buffer)
    {
        const auto data = buffer.data();
        const auto length = buffer.size();
        // m_count - 1 is the CheckSum, which a successful scan found after every payload.
        const auto checkSumPosition = static_cast<position_t>(m_fields[m_count - 1].m_position);
        for (size_t i = MessageTypePosition + 1; i + 2 < m_count; ++i)
        {
            const auto dataTag = DataFields::dataTag(m_tags[i]);
            if (dataTag < 0)
            {
                continue;
            }
            const auto& lengthField = m_fields[i];
            auto& dataField = m_fields[i + 1];
            if (dataField.m_tag != dataTag || lengthField.m_length == 0 ||
                static_cast<size_t>(lengthField.m_position) + lengthField.m_length > length)
            {
                return false;
            }
            const bool padded = lengthField.m_length <= 8 &&
                static_cast<size_t>(lengthField.m_position) + sizeof(uint64_t) <= length;
            const auto payloadLength = utils::asciiToUint64(data + lengthField.m_position, lengthField.m_length, padded);
            // The payload ends with the SOH at payloadEnd, before the CheckSum field.
            const auto payloadEnd = static_cast<uint64_t>(dataField.m_position) + payloadLength;
            if (payloadEnd >= checkSumPosition || data[payloadEnd] != FieldEnd)
            {
                return false;
            }
            // A token made from payload bytes starts at most one past the payload's
            // SOH (digits ending the payload at a block boundary); a real tag's value
            // starts at least three past it.
            size_t next = i + 2;
            while (next < m_count && m_fields[next].m_position <= payloadEnd + 1)
            {
                ++next;
            }
            dataField.m_length = static_cast<uint16_t>(payloadLength);
            if (const auto dropped = next - (i + 2); dropped > 0)
            {
                std::memmove(&m_fields[i + 2], &m_fields[next], (m_count - next) * sizeof(Field));
                std::memmove(&m_tags[i + 2], &m_tags[next], (m_count - next) * sizeof(uint16_t));
                m_count -= dropped;
            }
            ++i;
        }
        return true;
    }

    /**
     * Rescans from the first data length tag (after the header's BeginString,
     * BodyLength and MsgType) with data payloads skipped. The
     * tokens before it are kept, and the scan restarts at the tag's first
     * digit in the state a data skip leaves: no split tag carried over, and
     * blockSum covering exactly the bytes before the restart. Kept out of line
     * so the data-free path stays small.
     * @param buffer raw FIX message bytes
     * @param first the first scan's result, kept if the tag cannot be located
     * @return as for parse()
     */
    [[gnu::noinline]] ParseResult rescanFromDataField(const Buffer buffer, const ParseResult first)
    {
        const auto data = buffer.data();
        const auto length = static_cast<length_t>(buffer.size());
        size_t index = MessageTypePosition + 1;
        while (index < m_count && DataFields::dataTag(m_tags[index]) < 0)
        {
            ++index;
        }
        if (index >= m_count)
        {
            return first;
        }
        // The tag's digits end just before the '=' preceding its value.
        const auto previous = static_cast<position_t>(m_fields[index - 1].m_position);
        const auto valueStart = static_cast<position_t>(m_fields[index].m_position);
        if (valueStart < 2 || valueStart > length || data[valueStart - 1] != TagEnd)
        {
            return first;
        }
        position_t start = valueStart - 1;
        while (start > previous && data[start - 1] >= '0' && data[start - 1] <= '9')
        {
            --start;
        }
        if (start == valueStart - 1 || start <= previous || data[start - 1] != FieldEnd)
        {
            return first;
        }

        m_count = index;
        m_tag = 0;
        m_position = 0;
        uint64_t blockSum = 0;
        position_t i = 0;
        for (; i + Uint8x16::Size <= start; i += Uint8x16::Size)
        {
            m_block.load(data + i);
            blockSum += m_block.sum();
        }
        for (; i < start; ++i)
        {
            blockSum += data[i];
        }
        return tokenize<true>(buffer, start, blockSum, 0);
    }

    /**
     * Branch-free, since almost every message has no data field.
     * @return true if any token is a DataFields length tag
     */
    [[nodiscard]] bool hasDataLengthTag() const
    {
        bool found = false;
        for (size_t i = 0; i < m_count; ++i)
        {
            found |= DataFields::dataTag(m_tags[i]) >= 0;
        }
        return found;
    }

    /**
     * Validates BodyLength against the actual message size and checks for
     * the minimum required field count and MsgType tag, then verifies the
     * checksum via processCheckSum.
     * @param data raw message bytes
     * @param blockSum running sum of bytes covered by the SIMD scan, used
     *        to derive the checksum
     * @param blockEnd offset one past the last byte covered by blockSum
     * @param messageLength total buffer length, used to bound the checksum read
     * @return Result::Success with the number of bytes processed, or the
     *         first validation failure
     */
    ParseResult checkRequiredFields(const data_t* data, const uint64_t blockSum,
        const position_t blockEnd, const length_t messageLength) const
    {
        const auto* last = &m_fields[m_count - 1];
        const bool hasCheckSum = last->m_tag == CheckSumTag &&
            static_cast<uint32_t>(last->m_position) + last->m_length + 1 <= messageLength;
        const uint32_t processed = hasCheckSum ? last->m_position + last->m_length + 1 : 0;
        const auto& bodyLength = m_fields[BodyLengthPosition];
        if (static_cast<uint32_t>(bodyLength.m_position) + bodyLength.m_length > messageLength)
        {
            return {processed, Result::InvalidBodyLength};
        }
        const bool bodyPadded = bodyLength.m_length <= 8 &&
            static_cast<uint32_t>(bodyLength.m_position) + sizeof(uint64_t) <= messageLength;
        const int64_t length = utils::asciiToUint64(0, data + bodyLength.m_position, bodyLength.m_length, bodyPadded);
        const int32_t byteCount = last->m_position - bodyLength.m_position - bodyLength.m_length - 4;
        if (hasCheckSum)
        {
            if (bodyLength.m_tag != BodyLengthTag)
            {
                return {processed, Result::InvalidBodyLengthTag};
            }
            if (byteCount != length)
            {
                return {processed, Result::InvalidBodyLength};
            }
            if (m_fields[MessageTypePosition].m_tag != MessageTypeTag)
            {
                return {processed, Result::InvalidMessageTypeTag};
            }
            if (m_count < RequiredFieldCount)
            {
                return {processed, Result::RequiredFieldMissing};
            }
        }
        else if (byteCount < length)
        {
            return {0, Result::MessageFragment};
        }
#if defined(SIMDFIX_TRACE)
        for (size_t i = 0; i < m_count; ++i)
        {
            std::printf("%3zu tag = %3d, pos = %3d, len = %3d\n", i,
                m_fields[i].m_tag, m_fields[i].m_position, m_fields[i].m_length);
        }
#endif
        return {processed, processCheckSum(data, blockSum, blockEnd, messageLength)};
    }

    /**
     * Consumes one 16-byte block's worth of tag digits: closes out the
     * previous token (handling a tag split across the block boundary),
     * extracts each tag number found in this block into new tokens, and
     * carries over a trailing partial tag to the next block.
     * @param tagDigitFlags bitmask with a 4-bit-aligned nibble set for each
     *        byte position in this block that is part of a tag number
     * @param digits ASCII digit value at each lane where validTags was set,
     *        zero elsewhere
     * @param nonTagBitPos the bit position to resume scanning from; nonzero
     *        only for the first block, to skip the already-consumed
     *        BeginString
     * @param data raw message bytes
     * @param length message length
     * @param offset current block offset, updated on data skip
     * @param blockSum running byte sum, corrected on data skip
     * @tparam SkipData whether to skip data payloads (see parse())
     * @return true once the CheckSum tag (10) has been tokenized
     */
    template <bool SkipData>
    bool processBlock(const uint64_t tagDigitFlags,
                      const data_t* digits,
                      position_t nonTagBitPos,
                      const data_t* data,
                      const length_t length,
                      position_t& offset,
                      uint64_t& blockSum)
    {
        const auto trailingTagFlags = static_cast<uint16_t>(tagDigitFlags >> 48);
        const auto trailingCount = std::countl_one(trailingTagFlags);
        auto* field = &m_fields[m_count - 1];
        if (m_tag != 0 && (tagDigitFlags & 0xF) == 0)
        {  // split tag ending in first position of next block
            field->m_length = static_cast<int16_t>(m_position + offset - 1 - field->m_position);
            if (skipDataField<SkipData>(data, length, field, offset, blockSum))
            {
                return false;
            }
            ++m_count;
            ++field;
            field->m_tag = static_cast<uint16_t>(m_tag);
            m_tags[field - m_fields.data()] = static_cast<uint16_t>(m_tag);
            field->m_position = static_cast<uint16_t>(offset + 1);
            m_position = 0;
            m_tag = 0;
        }
        field->m_length = static_cast<uint16_t>(m_position);

        uint64_t remainingDigitFlags = tagDigitFlags & ~0ull >> std::max(4, trailingCount);
        while (remainingDigitFlags > 0 && field->m_tag != CheckSumTag) [[likely]]
        {
            const int32_t nonTagCount = std::countr_zero(remainingDigitFlags);
            nonTagBitPos += nonTagCount;
            remainingDigitFlags >>= nonTagCount;

            const position_t digitBits = std::countr_one(remainingDigitFlags);
            const position_t tagPos = nonTagBitPos >> 2;
            field->m_length += offset + tagPos - field->m_position - 1;

            if (skipDataField<SkipData>(data, length, field, offset, blockSum))
            {
                return false;
            }
            field = &m_fields[m_count++];
            field->m_length = 0;

            const position_t count = digitBits >> 2;
            const data_t* digit = &digits[tagPos];
            uint32_t value = 0;
            if (m_tag != 0)
            { // split tag carry-over
                value = m_tag;
                m_tag = 0;
            }
            field->m_tag = static_cast<uint16_t>(utils::asciiToUin32(value, digit, count));
            m_tags[field - m_fields.data()] = field->m_tag;
            field->m_position = static_cast<uint16_t>(offset + tagPos + count + 1);
            remainingDigitFlags >>= digitBits;
            nonTagBitPos += digitBits;
        }
        m_position = 0;
        // Last full block ending on SOH: the field it closes is still open, and the
        // trailer starts on the next tag's digits, so processTrailer would take its
        // slot for that tag instead of closing it. Close it here and leave the slot.
        // The field's value may have started in an earlier block, so its position is
        // not bounded by offset.
        if (offset + Uint8x16::Size + 15 >= length && field->m_tag != CheckSumTag &&
            data[offset + Uint8x16::Size - 1] == FieldEnd)
        {
            field->m_length = static_cast<uint16_t>(offset + Uint8x16::Size - 1 - field->m_position);
            m_fields[m_count] = {};
            m_tags[m_count] = 0;
            ++m_count;
        }
        if (trailingCount >= 4)
        {
            const auto count = trailingCount >> 2;
            const auto digit = &digits[Uint8x16::Size - count];
            m_tag = utils::asciiToUin32(0, digit, count);
            m_position = -count; // adjust position of next field
        }
        return m_fields[m_count - 1].m_tag == 10;
    }

    /**
     * If the just-completed token is a length tag for a data field, parses its
     * value, emits a synthetic token for the binary-safe data field, advances
     * offset past the data payload, and re-bases blockSum so the checksum stays
     * exact across the bytes the SIMD scan skips (or would double-count).
     * @tparam SkipData false compiles the check out (see parse())
     * @param data raw message bytes
     * @param length message length; bounds every read so a truncated/malformed
     *        message cannot read past the buffer
     * @param field the just-completed length tag token
     * @param offset current block offset; on a skip, repositioned so the next
     *        16-byte load resumes just past the data payload
     * @param blockSum running checksum byte sum, corrected for the skipped region
     * @return true if a data skip was emitted (the caller resumes the scan loop)
     */
    template <bool SkipData>
    bool skipDataField(const data_t* data, const length_t length, const Field* field,
                       position_t& offset, uint64_t& blockSum)
    {
        if constexpr (!SkipData)
        {
            return false;
        }
        const auto tag = DataFields::dataTag(field->m_tag);
        if (tag < 0)
        {
            return false;
        }
        // Bail if the length field's own digits fall outside the buffer
        // (truncated/malformed message); checkRequiredFields then rejects it.
        if (static_cast<uint32_t>(field->m_position) + field->m_length > length)
        {
            return false;
        }
        // Use the padded SWAR path only when 8 bytes can be read in-bounds.
        const bool padded = field->m_length <= 8 &&
            static_cast<uint32_t>(field->m_position) + sizeof(uint64_t) <= length;
        const auto lengthValue = static_cast<uint32_t>(
            utils::asciiToUint64(data + field->m_position, field->m_length, padded));

        // Skip the SOH after the length value and the data tag's "NNN=" prefix.
        auto pos = static_cast<position_t>(field->m_position + field->m_length + 1);
        while (pos < length && data[pos] != TagEnd)
        {
            ++pos;
        }
        ++pos;

        auto* next = &m_fields[m_count++];
        next->m_tag = static_cast<uint16_t>(tag);
        m_tags[next - m_fields.data()] = next->m_tag;
        next->m_position = static_cast<uint16_t>(pos);
        next->m_length = static_cast<int16_t>(lengthValue);

        // Resume just past the data payload's trailing SOH. Clamp to the buffer
        // so a huge (malformed) length cannot push the scan offset out of range;
        // checkRequiredFields rejects the message.
        const position_t skipEnd = std::min(pos + lengthValue + 1, static_cast<position_t>(length));

        // blockSum covers [0, blockEnd); re-base it to [0, skipEnd) so the
        // checksum stays exact across the skipped region. At most one loop runs.
        const position_t blockEnd = offset + Uint8x16::Size;
        for (position_t i = skipEnd; i < blockEnd; ++i)
        {
            blockSum -= data[i];
        }
        for (position_t i = blockEnd; i < skipEnd; ++i)
        {
            blockSum += data[i];
        }
        offset = skipEnd - Uint8x16::Size;
        m_tag = 0;
        m_position = 0;
        return true;
    }

    /**
     * Validates the trailing CheckSum field (tag 10) format and value
     * against the sum of all preceding bytes.
     * @param data raw message bytes
     * @param blockSum running sum of bytes covered by the SIMD scan, used
     *        as the starting point for the checksum
     * @param blockEnd offset one past the last byte covered by blockSum
     * @param length total buffer length, used to bound the checksum read
     * @return Result::Success, Result::InvalidCheckSumTag if the trailing
     *         bytes are not "\x01""10=...", or Result::InvalidCheckSum if
     *         the value does not match
     */
    Result processCheckSum(const std::span<const data_t>::pointer data,
                           const uint64_t blockSum,
                           const position_t blockEnd,
                           const length_t length) const
    {
        const auto& checkSumToken = m_fields[m_count - 1];
        // On a malformed/truncated message the checksum token can be positioned
        // at or past the buffer end; the 8-byte read below covers [pos-4, pos+4).
        if (checkSumToken.m_position < 4 ||
            static_cast<uint32_t>(checkSumToken.m_position) + 4 > length)
        {
            return Result::InvalidCheckSumTag;
        }
        uint64_t checks = 0;
        std::memcpy(&checks, data + checkSumToken.m_position - 4, sizeof(uint64_t));
        if ((checks & CheckSumMask) != CheckSumMask)
        {
            return Result::InvalidCheckSumTag;
        }

        // blockSum covers [0, blockEnd); the checksum covers [0, checkSumEnd),
        // so correct for the difference (at most one block plus the checksum field).
        // Clamp blockEnd to the buffer: a data-skip can leave it past the end.
        uint64_t checkSumValue = blockSum;
        const uint32_t checkSumEnd = checkSumToken.m_position - 3;
        const uint32_t scanEnd = std::min(blockEnd, static_cast<position_t>(length));
        for (uint32_t position = checkSumEnd; position < scanEnd; ++position)
        {
            checkSumValue -= data[position];
        }
        for (uint32_t position = scanEnd; position < checkSumEnd; ++position)
        {
            checkSumValue += data[position];
        }

        const auto messageCheckSum = checkSumValue & 0xff;
        if (utils::asciiToUint64(0, data + m_fields[m_count - 1].m_position,
            3, false) != messageCheckSum)
        {
            return Result::InvalidCheckSum;
        }
        return Result::Success;
    }

    /**
     * Tokenizes the final, less-than-16-byte tail of the message. Loads the
     * remaining bytes into a single uint64_t and uses bitmasks of
     * TagEnd ('=') and FieldEnd (SOH) byte positions to walk the remaining
     * tag=value pairs, handling a tag or value split across the previous
     * block boundary and finally emitting the CheckSum token (tag 10).
     * @param offset byte offset of the tail within the message
     * @param buffer raw FIX message bytes
     */
    void processTrailer(const position_t offset, const Buffer buffer)
    {
        auto* last = &m_fields[m_count - 1];
        const uint8_t* data = buffer.data() + offset;
        const size_t remaining = buffer.size() - offset;
#if defined(SIMDFIX_TRACE)
        utils::print(static_cast<uint32_t>(remaining), data);
#endif
        uint64_t bytes = 0;
        std::memcpy(&bytes, data, std::min(remaining, sizeof(uint64_t)));
        uint64_t tagEnds = utils::findByte(TagEnd, bytes);
        uint64_t fieldEnds = utils::findByte(FieldEnd, bytes);
        uint32_t tagEndBit = std::countr_zero(tagEnds);
        uint32_t tagEndPos = tagEndBit / 8;
        uint32_t fieldEndBit = std::countr_zero(fieldEnds);
        uint32_t fieldEndPos = fieldEndBit / 8;
        uint32_t position = 0;
        if (m_tag != 0)
        { // handle split tag
            last->m_length = static_cast<uint16_t>(offset + m_position - 1 - last->m_position);
            last = &m_fields[m_count++];
            const uint32_t tagBytes = std::min(tagEndPos, static_cast<uint32_t>(remaining));
            last->m_tag = static_cast<uint16_t>(utils::asciiToUint64(m_tag, data, tagBytes, false));
            m_tags[last - m_fields.data()] = last->m_tag;
            last->m_position = static_cast<uint16_t>(offset + tagEndPos + 1);
            last->m_length = static_cast<uint16_t>(fieldEndPos - tagEndPos - 1);
            position = fieldEndPos + 1;
            m_tag = 0;
        }
        else if (fieldEndPos < tagEndPos)
        { // handle split value
            last->m_length = static_cast<int16_t>(offset - last->m_position + fieldEndPos);
            position = fieldEndPos + 1;
            fieldEnds &= ~(1ULL << fieldEndBit);
            fieldEndBit = std::countr_zero(fieldEnds);
            fieldEndPos = fieldEndBit / 8;
            last = &m_fields[m_count++];
        }
        constexpr uint32_t CheckSumPrefixLen = 3; // "10="
        constexpr uint32_t CheckSumFieldLen = CheckSumPrefixLen + CheckSumValueLength + 1; // "10=" + digits + SOH
        while (position + CheckSumFieldLen < remaining && tagEndPos >= position)
        {
            last->m_tag = static_cast<uint16_t>(utils::asciiToUint64(0, data + position,
                tagEndPos - position, false));
            m_tags[last - m_fields.data()] = last->m_tag;
            position += tagEndPos - position + 1;
            last->m_position = static_cast<uint16_t>(position + offset);
            last->m_length = static_cast<uint16_t>(fieldEndPos - position);
            position += last->m_length + 1;
            last = &m_fields[m_count++];
            tagEnds &= ~(1ULL << tagEndBit);
            tagEndBit = std::countr_zero(tagEnds);
            tagEndPos = tagEndBit / 8;
            fieldEndBit = std::countr_zero(fieldEnds);
            fieldEndPos = fieldEndBit / 8;
            fieldEnds &= ~(1ULL << fieldEndBit);
        }
        if (position + CheckSumFieldLen <= remaining)
        {
            if (last->m_length >= 1)
            {   // last field have been used already
                last = &m_fields[m_count++];
            }
            last->m_position = static_cast<uint16_t>(offset + position + CheckSumPrefixLen);
            last->m_length = CheckSumValueLength;
            auto data = buffer.data() + offset + position;
            last->m_tag = (data[0] - '0') * 10 + (data[1] - '0');
        }
    }
};

}

#endif //SIMD_FIX_PAYLOAD_DECODER_HPP
