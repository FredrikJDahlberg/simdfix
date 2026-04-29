//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#include <algorithm>
#include <span>

#include "org/limitless/fix/parser/Tokenizer.hpp"

#include "ParserStatus.hpp"
#include "org/limitless/fix/parser/Utils.hpp"

namespace org::limitless::fix::parser {

Tokenizer::Result Tokenizer::scan(const std::span<const data_t> buffer)
{
    using simd::Uint8x16;
    m_count = 0;
    m_tag = 0;

    const auto data = buffer.data();
    const auto length = static_cast<length_t>(buffer.size());

    if (std::memcmp(data, "8=FIXT.1.1\x01", 10) != 0)
    {
        return { 8, 0, ParserStatus::InvalidBeginString };
    }
    m_tokens[0] = { 2, 8, 8 };
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
        print(16, data + offset);
#endif
        // A digit is valid if followed by '=' or a validated digit
        const Uint8x16 digitFlags{m_data >= ZerosBlock & m_data <= NinesBlock};
        const Uint8x16 tagEnds{m_data == TagEndsBlock};
        Uint8x16 after{digitFlags & tagEnds.shiftLeft<1>()};
        after |= digitFlags & after.shiftLeft<1>();
        after |= digitFlags & after.shiftLeft<1>();
        after |= digitFlags & after.shiftLeft<1>();
        // after |= digitFlags & after.shiftLeft<1>();

        // A digit is valid if preceded by 0x01 or a validated digit
        Uint8x16 fieldEnds{m_data == FieldEndsBlock};
        Uint8x16 before = digitFlags & fieldEnds.shiftRight<1>();
        before |= digitFlags & before.shiftRight<1>();
        before |= digitFlags & before.shiftRight<1>();
        before |= digitFlags & before.shiftRight<1>();
        // before |= digitFlags & before.shiftRight<1>();

        const Uint8x16 validTags{after | before};
        const Uint8x16 tags{validTags.whenTrue(m_data - ZerosBlock)};
        auto tagDigits = validTags.toUint64();  // 16 bytes to 4-bit nibble
        tags.get(0, digits);
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
        complete = processBlock(offset, tagDigits, digits, bits);
        bits = 0;
    }

    const auto& last = m_tokens[m_count - 1];
    Result result{ last.position + last.length + 1, 0, ParserStatus::Success };
    if (m_count < 7)
    {
        result.status = ParserStatus::RequiredFieldMissing;
        return result;
    }
    if (last.tag != CheckSumTag)
    {
        result.status = ParserStatus::InvalidCheckSumTag;
    }

    for (auto i = offset - 16; i < last.position - 3; i++)
    {
        checkSumValue += data[i];
    }
    result.checkSum = checkSumValue & 0xff;
    return result;
}

bool Tokenizer::processBlock(const position_t offset,
                             const uint64_t tagDigitFlags,
                             const data_t* digits,
                             position_t nonTagBitPos)
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
        token->tag = binaryToDecimal(value, digit, count);
        token->position = offset + tagPos + count + 1;
        remainingDigitFlags >>= digitBits;
        nonTagBitPos += digitBits;
    }
    m_position = 0;
    if (trailingCount >= 4)
    {
        const auto count = trailingCount >> 2;
        const auto digit = &digits[simd::Uint8x16::Size - count];
        m_tag = binaryToDecimal(0, digit, count);
        m_position = -count;
    }
    return m_tokens[m_count - 1].tag == 10;
}
}
