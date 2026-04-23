//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#include "org/limitless/fix/parser/Tokenizer.hpp"
#include "org/limitless/fix/parser/Utils.h"

#include <algorithm>
#include <span>

namespace org::limitless::fix::parser {
size_t Tokenizer::scan(const data_t* buffer, const length_t length)
{
    m_count = 0;
    m_tag = 0;
    m_tokens[0].position = 2;
    m_tokens[0].length = 8;
    m_tokens[0].tag = buffer[0] - '0';
    m_count = 1;
    uint64_t checkSum = 0;
    uint32_t bits = 4;
    position_t offset = 0;
    data_t digits[16];

    for (bool complete = false; offset + 15 < length && !complete; offset += 16)
    {
        m_data.put(buffer + offset, length - offset);
#if !defined(NDEBUG)
        print(16, buffer + offset);
#endif
        // A digit is valid if followed by '=' or a validated digit
        const simd::Uint8x16 digitFlags{m_data >= ZerosBlock & m_data <= NinesBlock};
        const simd::Uint8x16 tagEnds{m_data == TagEndsBlock};
        simd::Uint8x16 after{digitFlags & tagEnds.shiftLeft<1>()};
        after |= digitFlags & after.shiftLeft<1>();
        after |= digitFlags & after.shiftLeft<1>();
        after |= digitFlags & after.shiftLeft<1>();
        // after |= digitFlags & after.shiftLeft<1>();

        // A digit is valid if preceded by 0x01 or a validated digit
        simd::Uint8x16 fieldEnds{m_data == FieldEndsBlock};
        simd::Uint8x16 before = digitFlags & fieldEnds.shiftRight<1>();
        before |= digitFlags & before.shiftRight<1>();
        before |= digitFlags & before.shiftRight<1>();
        before |= digitFlags & before.shiftRight<1>();
        // before |= digitFlags & before.shiftRight<1>();

        const simd::Uint8x16 validTags{after | before};
        const simd::Uint8x16 tags{validTags.whenTrue(m_data - ZerosBlock)};
        auto tagDigits = validTags.toUint64();  // 16 bytes to 4-bit nibble
        tags.get(0, digits);
        tagDigits >>= bits;
#if !defined(NDEBUG)
        for (int i = 0; i < 16; ++i)
        {
            std::printf("%02x ", digits[i]);
        }
        std::printf("\n");
#endif
        complete = processBlock(offset, tagDigits, digits, bits);
        if (!complete)
        {
            checkSum += m_data.sum() & 0xff;
        }
        bits = 0;
    }

    checkRequiredFields(offset, checkSum, buffer);
    auto& token10 = m_tokens[m_count - 1];
    token10.length = 3;
    return token10.position + 4;
}

// this code is optimized for 4 digits
bool Tokenizer::processBlock(const position_t offset,
                        const uint64_t tagDigitFlags,
                        const data_t* digits,
                        position_t nonTagBitPos)
{
    const auto trailingTagFlags = static_cast<uint16_t>(tagDigitFlags >> 48);
    const int32_t trailingCount = std::countl_one(trailingTagFlags);
    auto* token = &m_tokens[m_count - 1];
    token->length = m_position;

    uint64_t remainingDigitFlags = tagDigitFlags & (~0ull >> std::max(4, trailingCount));
    while (remainingDigitFlags > 0 && token->tag != 10)
    {
        const int32_t nonTagCount = std::countr_zero(remainingDigitFlags);
        nonTagBitPos += nonTagCount;
        remainingDigitFlags >>= nonTagCount;

        const uint32_t digitBits = std::countr_one(remainingDigitFlags);
        const uint32_t tagPos = nonTagBitPos >> 2;
        token->length += offset + tagPos - token->position - 1;
        token = &m_tokens[m_count++];

        const uint32_t count = digitBits >> 2;
        const data_t* digit = &digits[tagPos];
        uint32_t value = 0;
        if (m_tag != 0)
        { // split tag carry-over
            value = m_tag;
            m_tag = 0;
        }
        token->tag = convertToDecimal(value, digit, count);
        token->position = offset + tagPos + count + 1;
        remainingDigitFlags >>= digitBits;
        nonTagBitPos += digitBits;
    }

    m_position = 0;
    if (trailingCount >= 4)
    {
        const int32_t count = trailingCount >> 2;
        m_position = -count;
        const data_t* digit = &digits[16 - count];
        m_tag = convertToDecimal(0, digit, count);
    }
    return m_tokens[m_count - 1].tag == 10;
}

void Tokenizer::checkRequiredFields(const position_t offset, data_t checkSum, const data_t* buffer) const
{
    if (std::memcmp(buffer, BeginString, 11) != 0)
    {
        throw std::invalid_argument("invalid begin string");
    }
    const auto& token35 = m_tokens[2];
    if (token35.tag != 35)
    {
        throw std::invalid_argument("invalid message type tag");
    }

    const auto& token10 = m_tokens[m_count - 1];
    if (token10.tag != 10)
    {
        throw std::invalid_argument("invalid check sum tag");
    }
    const auto& [position, tag, length] = m_tokens[1];
    if (tag != 9)
    {
        throw std::invalid_argument("invalid body length tag");
    }
    if (asciiToDecimal(buffer + position, length) != token10.position - token35.position)
    {
        throw std::invalid_argument("invalid body length");
    }

    const auto checkSumEnd = token10.position - 3;
    for (uint32_t i = offset - 16; i < checkSumEnd; i++)
    {
        checkSum += buffer[i];
    }
    checkSum &= 0xff;
    if (asciiToDecimal(buffer + checkSumEnd + 3, 3) != checkSum)
    {
        throw std::invalid_argument("invalid checksum");
    }
}
}
