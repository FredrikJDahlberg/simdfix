//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#ifndef SIMD_FIX_DECODER_HPP
#define SIMD_FIX_DECODER_HPP

#include <algorithm>
#include <bit>
#include <cstring>
#include <span>

#include "org/limitless/fix/decoder/Result.hpp"
#include "org/limitless/fix/decoder/Token.hpp"
#include "org/limitless/fix/simd/Uint8x16.hpp"
#include "org/limitless/fix/utils/BitSet64.hpp"
#include "org/limitless/fix/utils/Utils.hpp"

namespace org::limitless::fix::decoder {

class PayloadDecoder
{
    static constexpr size_t MaxSize = 64;

    Token m_tokens[MaxSize]{};
    uint16_t m_tags[MaxSize]{};

    simd::Uint8x16 m_block{};
    uint32_t m_tag{};
    int32_t m_position{};
    size_t m_count{};

public:
    using position_t = uint32_t;
    using length_t = uint16_t;
    using value_t = uint16_t;
    using data_t = uint8_t;

    static constexpr uint32_t BeginStringPosition = 0;
    static constexpr uint32_t BodyLengthPosition = 1;
    static constexpr uint32_t MessageTypePosition = 2;

    static constexpr uint32_t RequiredFieldCount = 7;
    static constexpr uint32_t MessageFragmentLimit = 32;

    static constexpr uint32_t CheckSumTag = 10;
    static constexpr uint32_t BodyLengthTag = 9;
    static constexpr uint32_t MessageTypeTag = 35;

    static constexpr uint8_t TagEnd = '=';
    static constexpr uint8_t FieldEnd = 0x01;

    static constexpr uint8_t BeginString[12] = { '8', '=', 'F', 'I', 'X', 'T', '.', '1', '.', '1', FieldEnd, '9' };
    static constexpr uint64_t CheckSumMask = 1 |  '1' << 8 | '0' << 16 | '=' << 24 | 1ULL << 56;

    static inline const simd::Uint8x16 TagEndsBlock{'='};
    static inline const simd::Uint8x16 FieldEndsBlock{0x01};
    static inline const simd::Uint8x16 ZerosBlock{'0'};
    static inline const simd::Uint8x16 NineMask{9};

    PayloadDecoder() noexcept = default;
    ~PayloadDecoder() = default;

    PayloadDecoder(const PayloadDecoder&) = delete;
    PayloadDecoder& operator=(const PayloadDecoder&) = delete;
    PayloadDecoder(PayloadDecoder&&) = delete;
    PayloadDecoder& operator=(PayloadDecoder&&) = delete;

    [[nodiscard]] std::span<Token> tokens() noexcept
    {
        return { m_tokens, m_count };
    }

    template <typename Handler>
    Result parse(const utils::Buffer buffer, Handler& handler)
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

    Result parse(const utils::Buffer buffer)
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
        if (std::memcmp(data, BeginString, sizeof(BeginString) - 1) != 0)
        {
            return { 8, Result::InvalidBeginString };
        }
        m_tokens[BeginStringPosition] = { 2, 8, 8 };
        m_count = 1;

        // Padded so binaryToDecimal can always read 8 bytes from any digits[0..15] start.
        data_t digits[Uint8x16::Size + sizeof(uint64_t)]{};
        position_t offset = 0;
        bool complete = false;
        position_t bits = 4;
        for (; offset + 15 < length && !complete; offset += Uint8x16::Size)
        {
            m_block.load(data + offset);
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
        for (size_t i = 0; i < m_count; ++i)
        {
            m_tags[i] = m_tokens[i].m_tag;
        }
        return checkRequiredFields(data);
    }

private:

    Result checkRequiredFields(const data_t* data) const
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
        const auto status = processCheckSum(data);
        if (status != Result::Success)
        {
            return {processed, status};
        }
        return {processed, m_count < 7 ? Result::RequiredFieldMissing : status};
    }

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
            token->m_position = offset + 1;
            m_position = 0;
            m_tag = 0;
        }
        token->m_length = m_position;

        uint64_t remainingDigitFlags = tagDigitFlags & ~0ull >> std::max(4, trailingCount);
        while (remainingDigitFlags > 0 && token->m_tag != CheckSumTag)
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
            token->m_tag = utils::binaryToDecimal(value, digit, count);
            token->m_position = offset + tagPos + count + 1;
            remainingDigitFlags >>= digitBits;
            nonTagBitPos += digitBits;
        }
        m_position = 0;
        if (trailingCount >= 4)
        {
            const auto count = trailingCount >> 2;
            const auto digit = &digits[simd::Uint8x16::Size - count];
            m_tag = utils::binaryToDecimal(0, digit, count);
            m_position = -count;
        }
        return m_tokens[m_count - 1].m_tag == 10;
    }

    Result::Values processCheckSum(const std::span<const data_t>::pointer data) const
    {
        uint64_t checks = 0;
        memcpy(&checks, data + m_tokens[m_count - 1].m_position - 4, sizeof(uint64_t));
        const auto& checkSumToken = m_tokens[m_count - 1];
        if ((checks & CheckSumMask) != CheckSumMask)
        {
            return Result::InvalidCheckSumTag;
        }

        uint64_t checkSumValue = 0;
        const uint32_t checkSumEnd = checkSumToken.m_position - 3;
        uint32_t position = 0;
        simd::Uint8x16 block;
        for (; position + simd::Uint8x16::Size <= checkSumEnd; position += simd::Uint8x16::Size)
        {
            block.load(data + position);
            checkSumValue += block.sum();
        }
        for (; position < checkSumEnd; ++position)
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

    void processTrailer(const position_t offset, const utils::Buffer buffer)
    {
#if !defined(NDEBUG)
        utils::print(buffer.size() % 16, buffer.data() + offset);
#endif
        auto* last = &m_tokens[m_count - 1];
        const uint8_t* data = buffer.data() + offset;
        const size_t remaining = buffer.size() - offset;
        uint64_t bytes = 0;
        memcpy(&bytes, data, std::min(remaining, sizeof(uint64_t)));
        auto tagEnds = BitSet64{utils::findByte(TagEnd, bytes)};
        auto fieldEnds = BitSet64{utils::findByte(FieldEnd, bytes)};
        uint32_t tagEndBit = tagEnds.zerosRight();
        uint32_t tagEndPos = tagEndBit / 8;
        uint32_t fieldEndBit = fieldEnds.zerosRight();
        uint32_t fieldEndPos = fieldEndBit / 8;
        uint32_t position = 0;
        if (m_tag != 0)
        { // handle split tag
            last->m_length = offset + m_position - 1 - last->m_position;
            last = &m_tokens[m_count++];
            last->m_tag = utils::asciiToUint64(m_tag, data, tagEndPos, false);
            last->m_position = offset + tagEndPos + 1;
            last->m_length = fieldEndPos - tagEndPos - 1;
            position = fieldEndPos + 1;
            m_tag = 0;
        }
        else if (fieldEndPos < tagEndPos)
        { // handle split value
            position = fieldEndPos + 1;
            fieldEndPos = fieldEnds.clear(fieldEndBit).zerosRight() / 8;
            last->m_length += fieldEndPos - tagEndPos - 1;
            last = &m_tokens[m_count++];
        }
        while (position + 7 < remaining)
        {
            last->m_tag = utils::asciiToUint64(0, data + position, tagEndPos - position, false);
            position += tagEndPos - position + 1;
            last->m_position = position + offset;
            last->m_length = fieldEndPos - position;
            position += last->m_length + 1;
            last = &m_tokens[m_count++];
            tagEndBit = tagEnds.clear(tagEndBit).zerosRight();
            tagEndPos = tagEndBit / 8;
            fieldEndBit = fieldEnds.zerosRight();
            fieldEndPos = fieldEndBit / 8;
            fieldEnds.clear(fieldEndBit);
        }
        if (position < remaining)
        { // checked by decoder
            last = &m_tokens[m_count++];
            last->m_tag = 10;
            last->m_position = offset + position + 3;
            last->m_length = 3;
        }
    }
};
}

#endif //SIMD_FIX_DECODER_H
