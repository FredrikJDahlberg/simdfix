//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#include "org/limitless/fix/parser/Tokenizer.hpp"

#include <algorithm>
#include <span>

namespace org::limitless::fix::parser {

size_t Tokenizer::scan(const uint8_t* buffer, const int32_t length)
{
    m_count = 0;
    m_tag = 0;

    m_tokens[0] = { 2, static_cast<uint16_t>(buffer[0] - '0'), 8 };
    m_count = 1;
    uint64_t checkSum = 0;
    uint32_t bits = 4;
    int32_t offset = 0;
    uint8_t digits[16];
    bool complete = false;
    for (; offset + 15 < length && !complete; offset += 16)
    {
        m_data.put(buffer + offset, length - offset);
#if !defined(NDEBUG)
        dump(16, buffer + offset);
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
        complete = process(offset, tagDigits, digits, bits);
        if (!complete)
        {
            checkSum += m_data.sum();
        }
        bits = 0;
    }

    const auto& token8 = m_tokens[0];
    if (token8.tag != 8)
    {
        throw std::invalid_argument("invalid begin string tag");
    }
    const auto& token9 = m_tokens[1];
    if (token9.tag != 9)
    {
        throw std::invalid_argument("invalid body length tag");
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
    const auto bodyLength = token10.position - token35.position;
    if (asciiToDecimal(buffer, token9.position, token9.length) != bodyLength)
    {
        throw std::invalid_argument("invalid body length");
    }

    const auto checkSumEnd = token10.position - 3;
    for (uint32_t i = offset - 16; i < checkSumEnd; i++)
    {
        checkSum += buffer[i];
    }
    checkSum &= 0xff;
    if (asciiToDecimal(buffer, checkSumEnd + 3, 3) != checkSum)
    {
        throw std::invalid_argument("invalid checksum");
    }

    return checkSumEnd + 7;
}

bool Tokenizer::process(const int32_t offset, const uint64_t tagDigitFlags, const uint8_t* digits, uint32_t nonTagBitPos)
{
#if !defined(NDEBUG)
    for (int i = 0; i < 16; ++i)
    {
        std::printf("%02x ", digits[i]);
    }
    std::printf("\n");
#endif
    const uint16_t trailingTagFlags = tagDigitFlags >> 48; // 52
    const int32_t trailingCount = std::countl_one(trailingTagFlags);
    uint64_t remainingDigitFlags = tagDigitFlags & (-1ull >> std::max(4, trailingCount));

    auto token = m_tokens + m_count - 1;
    token->length = m_position;
    while (remainingDigitFlags > 0 && token->tag != 10)
    {
        const int32_t nonTagCount = std::countr_zero(remainingDigitFlags);
        nonTagBitPos += nonTagCount;
        remainingDigitFlags >>= nonTagCount;
        const auto digitBits = std::countr_one(remainingDigitFlags);
        const int32_t digitCount = digitBits / 4;
        const uint32_t tagPosition = nonTagBitPos / 4;
        token->length += offset + tagPosition - token->position - 1;
        auto& [position, tag, length] = m_tokens[m_count];
        ++m_count;
        ++token;

        tag = convertToDecimal(m_tag, digits, tagPosition, digitCount);
        m_tag = 0;
        position = offset + tagPosition + digitCount + 1;
        remainingDigitFlags >>= digitBits;
        nonTagBitPos += digitBits;
    }
    m_position = 0;
    if (trailingCount >= 4)
    {
        const auto digitCount = trailingCount / 4;
        m_position = -digitCount;
        m_tag = convertToDecimal(0, digits, 16 - digitCount, digitCount);
    }
    return token->tag == 10;
}

}
