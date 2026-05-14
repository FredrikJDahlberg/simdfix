//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#include <algorithm>
#include <span>

#include "org/limitless/fix/parser/Tokenizer.hpp"

#include <chrono>

#include "org/limitless/fix/utils/BitSet64.hpp"
#include "org/limitless/fix/parser/ParserStatus.hpp"
#include "org/limitless/fix/utils/Utils.hpp"

namespace org::limitless::fix::parser {

Tokenizer::Result Tokenizer::scan(const std::span<const data_t> buffer, uint16_t* tags)
{
    if (buffer.size() < 32)
    {
        return { 0, 0, ParserStatus::MessageFragment };
    }

    using simd::Uint8x16;
    m_count = 0;
    m_tag = 0;

    const auto data = buffer.data();
    const auto length = static_cast<length_t>(buffer.size());

    if (std::memcmp(data, BeginString, sizeof(BeginString) - 1) != 0)
    {
        return { 8, 0, ParserStatus::InvalidBeginString };
    }
    m_tokens[0] = { 2, 8, 8 };
    tags[0] = 8;
    m_count = 1;

    position_t bits = 4;
    data_t digits[Uint8x16::Size];
    uint8_t lastSum = 0;
    position_t offset = 0;
    bool complete = false;
    uint32_t checkSumValue = 0;
    for (; offset + 15 < length && !complete; offset += Uint8x16::Size)
    {
        m_data.put(data + offset, length - offset);
#if !defined(NDEBUG)
        utils::print(16, data + offset);
#endif
        // A digit is valid if followed by '=' or a validated digit
        const Uint8x16 digitFlags{m_data >= ZerosBlock & m_data <= NinesBlock};
        const Uint8x16 tagEnds{m_data == TagEndsBlock};
        Uint8x16 after{digitFlags & tagEnds.shiftLeft<1>()};
        after |= digitFlags & after.shiftLeft<1>();
        after |= digitFlags & after.shiftLeft<1>();
        after |= digitFlags & after.shiftLeft<1>();
        after |= digitFlags & after.shiftLeft<1>();

        // A digit is valid if preceded by 0x01 or a validated digit
        Uint8x16 fieldEnds{m_data == FieldEndsBlock};
        Uint8x16 before = digitFlags & fieldEnds.shiftRight<1>();
        before |= digitFlags & before.shiftRight<1>();
        before |= digitFlags & before.shiftRight<1>();
        before |= digitFlags & before.shiftRight<1>();
        before |= digitFlags & before.shiftRight<1>();

        const Uint8x16 validTags{after | before};
        const Uint8x16 tagsBlock{validTags.whenTrue(m_data - ZerosBlock)};
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
        lastSum = m_data.sum() & 0xff;
        checkSumValue += lastSum;
        complete = processBlock(offset, tagDigits, digits, bits, tags);
        bits = 0;
    }

    Result result{ 0, 0, ParserStatus::Success };
    if (complete)
    {
        m_tokens[m_count - 1].length = 3; // previous field is checksum
    }
    else
    {
        processTrailer(offset, buffer, tags);
    }
    if (m_count < 7)
    {
        result.status = ParserStatus::RequiredFieldMissing;
        return result;
    }

    uint64_t checks = 0;
    memcpy(&checks, data + m_tokens[m_count - 1].position - 4, sizeof(uint64_t));
    const auto& checkSum = m_tokens[m_count - 1];
    if ((checks & CheckSumMask) != CheckSumMask)
    {
        std::println("error: tag={}, pos={}, len={}", checkSum.tag, checkSum.position, checkSum.length);
        std::println("check: {:016x}, mask={:016x}, res={:016x}", checks, CheckSumMask, checks&CheckSumMask);
        result.status = ParserStatus::InvalidCheckSumTag;
        result.processed = checkSum.position + checkSum.length + 1;
        return result;
    }

    checkSumValue -= lastSum;
    for (auto i = offset - 16; i < checkSum.position - 3; i++)
    {
        checkSumValue += data[i];
    }
    result.checkSum = checkSumValue & 0xff;
    result.processed = checkSum.position + checkSum.length + 1;
    return result;
}

bool Tokenizer::processBlock(const position_t offset,
                             const uint64_t tagDigitFlags,
                             const data_t* digits,
                             position_t nonTagBitPos,
                             uint16_t* tags)
{
    const auto trailingTagFlags = static_cast<uint16_t>(tagDigitFlags >> 48);
    const auto trailingCount = std::countl_one(trailingTagFlags);
    auto* token = &m_tokens[m_count - 1];
    if (m_tag != 0 && digits[0] == 0)
    {  // split tag ending in first position of next block
        token->length = static_cast<int16_t>(m_position + offset - 1 - token->position);
        ++m_count;
        ++token;
        token->tag = m_tag;
        token->position = offset + 1;
        tags[m_count - 1] = token->tag;
        m_position = 0;
        m_tag = 0;
    }
    token->length = m_position;

    uint64_t remainingDigitFlags = tagDigitFlags & (~0ull >> std::max(4, trailingCount));
    while (remainingDigitFlags > 0 && token->tag != CheckSumTag)
    {
        const int32_t nonTagCount = std::countr_zero(remainingDigitFlags);
        nonTagBitPos += nonTagCount;
        remainingDigitFlags >>= nonTagCount;

        const position_t digitBits = std::countr_one(remainingDigitFlags);
        const position_t tagPos = nonTagBitPos >> 2;
        token->length += offset + tagPos - token->position - 1;
        token = &m_tokens[m_count++];

        const position_t count = digitBits >> 2;
        const data_t* digit = &digits[tagPos];
        uint32_t value = 0;
        if (m_tag != 0)
        { // split tag carry-over
            value = m_tag;
            m_tag = 0;
        }
        token->tag = utils::binaryToDecimal(value, digit, count);
        tags[m_count - 1] = token->tag;
        token->position = offset + tagPos + count + 1;
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
    return m_tokens[m_count - 1].tag == 10;
}

void Tokenizer::processTrailer(const position_t offset, const std::span<const uint8_t> buffer, uint16_t* tags)
{
#if !defined(NDEBUG)
    utils::print(buffer.size() % 16, buffer.data() + offset);
#endif
    auto* last = &m_tokens[m_count - 1];
    const uint8_t* data = buffer.data() + offset;
    uint64_t bytes;
    memcpy(&bytes, data, sizeof(uint64_t));
    auto tagEnds = BitSet64{utils::findByte(TagEnd, bytes)};
    auto fieldEnds = BitSet64{utils::findByte(FieldEnd, bytes)};
    uint32_t tagEndBit = tagEnds.zerosRight();
    uint32_t tagEndPos = tagEndBit / 8;
    uint32_t fieldEndBit = fieldEnds.zerosRight();
    uint32_t fieldEndPos = fieldEndBit / 8;
    uint32_t position = 0;
    if (m_tag != 0)
    { // handle split tag
        last->length = offset + m_position - 1 - last->position;
        last = &m_tokens[m_count++];
        last->tag = utils::asciiToDecimal(m_tag, data, tagEndPos);
        last->position = offset + tagEndPos + 1;
        last->length = fieldEndPos - tagEndPos - 1;
        position = fieldEndPos + 1;
        m_tag = 0;
    }
    else if (fieldEndPos < tagEndPos)
    { // handle split value
        fieldEndPos = fieldEnds.clear(fieldEndBit).zerosRight() / 8;
        last->length += fieldEndPos - tagEndPos - 1;
        position = fieldEndPos + 1;
        last = &m_tokens[m_count++];
    }
    while (position + 7 < buffer.size() - offset)
    {
        last->tag = utils::asciiToDecimal(0, data + position, tagEndPos - position);
        position += tagEndPos - position + 1;
        last->position = position + offset;
        last->length = fieldEndPos - position;
        position += last->length + 1;
        last = &m_tokens[m_count++];
        tagEndBit = tagEnds.clear(tagEndBit).zerosRight();
        tagEndPos = tagEndBit / 8;
        fieldEndBit = fieldEnds.zerosRight();
        fieldEndPos = fieldEndBit / 8;
        fieldEnds.clear(fieldEndBit);
    }
    if (position < buffer.size() - offset)
    { // checked by decoder
        last = &m_tokens[m_count++];
        last->tag = 10;
        last->position = offset + position + 3;
        last->length = 3;
    }
}
}

