//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#ifndef SIMD_FIX_DECODER_HPP
#define SIMD_FIX_DECODER_HPP

#include <algorithm>
#include <bit>
#include <cstring>
#include <span>

#include "org/limitless/fix/CodecTypes.hpp"
#include "org/limitless/fix/simd/Uint8x16.hpp"
#include "org/limitless/fix/utils/Utils.hpp"

namespace org::limitless::fix::decoder {

/**
 * SIMD tokenizer for raw FIX message bytes. Scans the buffer 16 bytes at a
 * time with NEON to locate tag/value boundaries ('=' and SOH), filling a
 * fixed-size Token[] array (position + tag + length, no copies) and a
 * parallel tag array. Also validates BeginString, BodyLength, and the
 * trailing CheckSum. Handles messages that are fragmented across chunk
 * boundaries, including tags split across a 16-byte block boundary.
 */
class PayloadDecoder
{
    static constexpr size_t MaxSize = 64;

    static constexpr uint32_t RequiredFieldCount = 7;
    static constexpr uint32_t MessageFragmentLimit = 32;

    static constexpr uint64_t CheckSumMask = 1 |  '1' << 8 | '0' << 16 | '=' << 24 | 1ULL << 56;

    static inline const simd::Uint8x16 TagEndsBlock{'='};
    static inline const simd::Uint8x16 FieldEndsBlock{0x01};
    static inline const simd::Uint8x16 ZerosBlock{'0'};
    static inline const simd::Uint8x16 NineMask{9};

    Token m_tokens[MaxSize]{};
    uint16_t m_tags[MaxSize]{};

    simd::Uint8x16 m_block{};
    uint32_t m_tag{};
    int32_t m_position{};
    size_t m_count{};

    uint8_t m_protocol[16];
    uint32_t m_protocolLength;

public:
    using position_t = uint32_t;
    using length_t = uint16_t;
    using value_t = uint16_t;
    using data_t = uint8_t;

    PayloadDecoder() = delete;

    explicit PayloadDecoder(const Protocol::Values protocol)
    {
        m_protocol[0] = '8';
        m_protocol[1] = '=';
        const auto code = Protocol::code(protocol);
        const auto size = code.size();
        std::memcpy(m_protocol + 2, code.data(), size);
        m_protocol[2 + size] = FieldEnd;
        m_protocolLength = size + 2;
    }

    ~PayloadDecoder() = default;

    PayloadDecoder(const PayloadDecoder&) = delete;
    PayloadDecoder& operator=(const PayloadDecoder&) = delete;
    PayloadDecoder(PayloadDecoder&&) = delete;
    PayloadDecoder& operator=(PayloadDecoder&&) = delete;

    /**
     * Used for testing.
     * @return the tokens produced by the most recent parse() call
     */
    [[nodiscard]] std::span<Token> tokens() noexcept
    {
        return { m_tokens, m_count };
    }

    /**
     * Tokenizes buffer and, on success, dispatches the decoded message to
     * handler by MsgType.
     * @param buffer raw FIX message bytes
     * @param handler receives the tokenized message via handle(); see
     *        MessageHandler
     * @return Result::Success and handler's status, or the tokenizer's
     *         failure status if parsing failed
     */
    template <typename Handler>
    Result parse(const Buffer buffer, Handler& handler)
    {
        auto result = parse(buffer);
        if (result.m_value != Result::Success)
        {
            return { result.m_processed, result.m_value };
        }

        const auto messageType = buffer[m_tokens[MessageTypePosition].m_position];
        result.m_value = handler.handle(buffer, std::span(m_tokens, m_count), std::span(m_tags, m_count), m_count, messageType);
        return result;
    }

    /**
     * Tokenizes buffer into m_tokens/m_tags: validates BeginString, scans
     * 16 bytes at a time to locate tag/value boundaries, and validates
     * BodyLength and CheckSum on completion.
     * @param buffer raw FIX message bytes
     * @return Result::Success with the number of bytes processed, or a
     *         failure status (e.g. Result::MessageFragment if buffer ends
     *         mid-message)
     */
    Result parse(const Buffer buffer)
    {
        using simd::Uint8x16;
        if (buffer.size() < MessageFragmentLimit)
        {
            return { 0, Result::MessageFragment };
        }
        m_count = 0;
        m_tag = 0;

        const auto data = buffer.data();
        const auto length = static_cast<length_t>(buffer.size());
        if (std::memcmp(data, m_protocol, m_protocolLength) != 0)
        {
            return { 8, Result::InvalidBeginString };
        }
        m_tokens[BeginStringPosition] = { 2, 8, 8 };
        m_tags[BeginStringPosition] = 8;
        m_count = 1;

        // Padded so binaryToDecimal can always read 8 bytes from any digits[0..15] start.
        data_t digits[Uint8x16::Size + sizeof(uint64_t)]{};
        position_t offset = 0;
        bool complete = false;
        position_t bits = 4;
        uint64_t blockSum = 0;
        for (; offset + 15 < length && !complete; offset += Uint8x16::Size)
        {
            m_block.load(data + offset);
            blockSum += m_block.sum();
#if !defined(NDEBUG)
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
#if !defined(NDEBUG)
            for (const auto digit : digits)
            {
                std::printf("%02x ", digit);
            }
            std::printf("\n");
#endif
            complete = processBlock(offset, tagDigits, digits, bits);
            bits = 0;
        }
        if (complete)
        {
            m_tokens[m_count - 1].m_length = 3; // previous field is checksum
        }
        else if (offset < buffer.size())
        {
            processTrailer(offset, buffer);
        }
        return checkRequiredFields(data, blockSum, offset);
    }

private:

    /**
     * Validates BodyLength against the actual message size and checks for
     * the minimum required field count and MsgType tag, then verifies the
     * checksum via processCheckSum.
     * @param data raw message bytes
     * @param blockSum running sum of bytes covered by the SIMD scan, used
     *        to derive the checksum
     * @param blockEnd offset one past the last byte covered by blockSum
     * @return Result::Success with the number of bytes processed, or the
     *         first validation failure
     */
    Result checkRequiredFields(const data_t* data, const uint64_t blockSum, const position_t blockEnd) const
    {
        const auto* last = &m_tokens[m_count - 1];
        const bool hasCheckSum = last->m_tag == CheckSumTag;
        const uint32_t processed = hasCheckSum ? last->m_position + last->m_length + 1 : 0;
        const auto& bodyLength = m_tokens[BodyLengthPosition];
        if (bodyLength.m_tag != BodyLengthTag)
        {
            return {processed, Result::InvalidBodyLengthTag};
        }

        const auto length = utils::asciiToUint64(0, data + bodyLength.m_position, bodyLength.m_length, true);
        const uint32_t byteCount = last->m_position - bodyLength.m_position - bodyLength.m_length - 4;
        if (!hasCheckSum && byteCount < length)
        {
            return {0, Result::MessageFragment};
        }
        if (byteCount != length)
        {
            return {processed, Result::InvalidBodyLength};
        }

        if (!hasCheckSum && m_count < RequiredFieldCount)
        {
            return {0, Result::RequiredFieldMissing};
        }
        if (m_tokens[MessageTypePosition].m_tag != MessageTypeTag)
        {
            return {processed, Result::InvalidMessageTypeTag};
        }

#if !defined(NDEBUG)
        for (size_t i = 0; i < m_count; ++i)
        {
            std::printf("%3zu tag = %3d, pos = %3d, len = %3d\n", i, m_tokens[i].m_tag, m_tokens[i].m_position, m_tokens[i].m_length);
        }
#endif


        // checksum and field count
        const auto status = processCheckSum(data, blockSum, blockEnd);
        if (status != Result::Success)
        {
            return {processed, status};
        }
        return {processed, m_count < 7 ? Result::RequiredFieldMissing : status};
    }

    /**
     * Consumes one 16-byte block's worth of tag digits: closes out the
     * previous token (handling a tag split across the block boundary),
     * extracts each tag number found in this block into new tokens, and
     * carries over a trailing partial tag to the next block.
     * @param offset byte offset of this block within the message
     * @param tagDigitFlags bitmask with a 4-bit-aligned nibble set for each
     *        byte position in this block that is part of a tag number
     * @param digits ASCII digit value at each lane where validTags was set,
     *        zero elsewhere
     * @param nonTagBitPos bit position to resume scanning from; nonzero
     *        only for the first block, to skip the already-consumed
     *        BeginString
     * @return true once the CheckSum tag (10) has been tokenized
     */
    bool processBlock(const position_t offset,
                      const uint64_t tagDigitFlags,
                      const data_t* digits,
                      position_t nonTagBitPos)
    {
        const auto trailingTagFlags = static_cast<uint16_t>(tagDigitFlags >> 48);
        const auto trailingCount = std::countl_one(trailingTagFlags);
        auto* token = &m_tokens[m_count - 1];
        if (m_tag != 0 && digits[0] == 0)
        {  // split tag ending in first position of next block
            token->m_length = static_cast<int16_t>(m_position + offset - 1 - token->m_position);
            ++m_count;
            ++token;
            token->m_tag = m_tag;
            m_tags[token - m_tokens] = m_tag;
            token->m_position = offset + 1;
            m_position = 0;
            m_tag = 0;
        }
        token->m_length = m_position;

        uint64_t remainingDigitFlags = tagDigitFlags & ~0ull >> std::max(4, trailingCount);
        while (remainingDigitFlags > 0 && token->m_tag != CheckSumTag) [[likely]]
        {
            const int32_t nonTagCount = std::countr_zero(remainingDigitFlags);
            nonTagBitPos += nonTagCount;
            remainingDigitFlags >>= nonTagCount;

            const position_t digitBits = std::countr_one(remainingDigitFlags);
            const position_t tagPos = nonTagBitPos >> 2;
            token->m_length += offset + tagPos - token->m_position - 1;
            token = &m_tokens[m_count++];

            const position_t count = digitBits >> 2;
            const data_t* digit = &digits[tagPos];
            uint32_t value = 0;
            if (m_tag != 0)
            { // split tag carry-over
                value = m_tag;
                m_tag = 0;
            }
            token->m_tag = utils::asciiToUin32(value, digit, count);
            m_tags[token - m_tokens] = token->m_tag;
            token->m_position = offset + tagPos + count + 1;
            remainingDigitFlags >>= digitBits;
            nonTagBitPos += digitBits;
        }
        m_position = 0;
        if (trailingCount >= 4)
        {
            const auto count = trailingCount >> 2;
            const auto digit = &digits[simd::Uint8x16::Size - count];
            m_tag = utils::asciiToUin32(0, digit, count);
            m_position = -count;
        }
        return m_tokens[m_count - 1].m_tag == 10;
    }

    /**
     * Validates the trailing CheckSum field (tag 10) format and value
     * against the sum of all preceding bytes.
     * @param data raw message bytes
     * @param blockSum running sum of bytes covered by the SIMD scan, used
     *        as the starting point for the checksum
     * @param blockEnd offset one past the last byte covered by blockSum
     * @return Result::Success, Result::InvalidCheckSumTag if the trailing
     *         bytes are not "\x01""10=...", or Result::InvalidCheckSum if
     *         the value does not match
     */
    Result::Values processCheckSum(const std::span<const data_t>::pointer data,
                                   const uint64_t blockSum,
                                   const position_t blockEnd) const
    {
        uint64_t checks = 0;
        std::memcpy(&checks, data + m_tokens[m_count - 1].m_position - 4, sizeof(uint64_t));
        const auto& checkSumToken = m_tokens[m_count - 1];
        if ((checks & CheckSumMask) != CheckSumMask)
        {
            return Result::InvalidCheckSumTag;
        }

        // blockSum covers [0, blockEnd); the checksum covers [0, checkSumEnd),
        // so correct for the difference (at most one block plus the checksum field).
        uint64_t checkSumValue = blockSum;
        const uint32_t checkSumEnd = checkSumToken.m_position - 3;
        for (uint32_t position = checkSumEnd; position < blockEnd; ++position)
        {
            checkSumValue -= data[position];
        }
        for (uint32_t position = blockEnd; position < checkSumEnd; ++position)
        {
            checkSumValue += data[position];
        }

        const auto messageCheckSum = checkSumValue & 0xff;
        if (utils::asciiToUint64(0, data + m_tokens[m_count - 1].m_position, 3, false) != messageCheckSum)
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
#if !defined(NDEBUG)
        utils::print(buffer.size() % 16, buffer.data() + offset);
#endif
        auto* last = &m_tokens[m_count - 1];
        const uint8_t* data = buffer.data() + offset;
        const size_t remaining = buffer.size() - offset;
        uint64_t bytes = 0;
        if (remaining >= sizeof(uint64_t))
        {
            std::memcpy(&bytes, data, sizeof(uint64_t));
        }
        else
        {
            for (size_t i = 0; i < remaining; ++i)
            {
                bytes |= static_cast<uint64_t>(data[i]) << (i * 8);
            }
        }
        uint64_t tagEnds = utils::findByte(TagEnd, bytes);
        uint64_t fieldEnds = utils::findByte(FieldEnd, bytes);
        uint32_t tagEndBit = std::countr_zero(tagEnds);
        uint32_t tagEndPos = tagEndBit / 8;
        uint32_t fieldEndBit = std::countr_zero(fieldEnds);
        uint32_t fieldEndPos = fieldEndBit / 8;
        uint32_t position = 0;
        if (m_tag != 0)
        { // handle split tag
            last->m_length = offset + m_position - 1 - last->m_position;
            last = &m_tokens[m_count++];
            last->m_tag = utils::asciiToUint64(m_tag, data, tagEndPos, false);
            m_tags[last - m_tokens] = last->m_tag;
            last->m_position = offset + tagEndPos + 1;
            last->m_length = fieldEndPos - tagEndPos - 1;
            position = fieldEndPos + 1;
            m_tag = 0;
        }
        else if (fieldEndPos < tagEndPos)
        { // handle split value: bytes from last->m_position to offset came from
          // the preceding block; fieldEndPos tail bytes complete the value.
            last->m_length = static_cast<int16_t>(offset - last->m_position + fieldEndPos);
            position = fieldEndPos + 1;
            fieldEnds &= ~(1ULL << fieldEndBit);
            fieldEndBit = std::countr_zero(fieldEnds);
            fieldEndPos = fieldEndBit / 8;
            last = &m_tokens[m_count++];
        }
        while (position + 7 < remaining)
        {
            last->m_tag = utils::asciiToUint64(0, data + position, tagEndPos - position, false);
            m_tags[last - m_tokens] = last->m_tag;
            position += tagEndPos - position + 1;
            last->m_position = position + offset;
            last->m_length = fieldEndPos - position;
            position += last->m_length + 1;
            last = &m_tokens[m_count++];
            tagEnds &= ~(1ULL << tagEndBit);
            tagEndBit = std::countr_zero(tagEnds);
            tagEndPos = tagEndBit / 8;
            fieldEndBit = std::countr_zero(fieldEnds);
            fieldEndPos = fieldEndBit / 8;
            fieldEnds &= ~(1ULL << fieldEndBit);
        }
        if (position < remaining)
        { // checked by decoder
            constexpr uint32_t checkSumPrefixLen = 3; // "10="
            last = &m_tokens[m_count++];
            last->m_tag = CheckSumTag;
            m_tags[last - m_tokens] = CheckSumTag;
            last->m_position = offset + position + checkSumPrefixLen;
            last->m_length = CheckSumValueLength;
        }
    }
};
}

#endif //SIMD_FIX_DECODER_H
