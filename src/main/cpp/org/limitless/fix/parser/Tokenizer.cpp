//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#include "org/limitless/fix/parser/Tokenizer.hpp"

namespace org::limitless::fix::parser {

size_t Tokenizer::scan(const uint8_t* buffer, const int32_t length)
{
    m_count = 0;
    m_tag = 0;

    m_tokens[0] = { 2, static_cast<uint16_t>(buffer[0] - '0'), 8 };
    m_count = 1;
    uint32_t bits = 4;
    uint64_t checkSum = 0;
    for (int32_t offset = 0; offset + 15 < length; offset += 16)
    {
        m_data.put(buffer + offset, length - offset);
        checkSum += m_data.sum();

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
        uint8_t digits[16];
        tags.get(0, digits);
        tagDigits >>= bits;
        process(offset, tagDigits, digits, bits);
        bits = 0;
    }
    // FIXME: adjust checksum and stop tokenizer after tag 10
    // std::printf("%3d\n", checkSum % 256);
    return 0; // FIXME: number of processed bytes
}

void Tokenizer::process(const int32_t offset, const uint64_t tagDigitFlags, const uint8_t* digits, uint32_t nonTagBitPos)
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
    m_tokens[m_count - 1].valueLength = m_position;
    while (remainingDigitFlags > 0)
    {
        const int32_t nonTagCount = std::countr_zero(remainingDigitFlags);
        nonTagBitPos += nonTagCount;
        remainingDigitFlags >>= nonTagCount;
        const auto digitBits = std::countr_one(remainingDigitFlags);
        const int32_t digitCount = digitBits / 4;
        const int32_t tagPosition = nonTagBitPos / 4;
        auto& prevToken = m_tokens[m_count - 1];
        prevToken.valueLength += offset + tagPosition - prevToken.valueOffset - 1;
#if !defined(NDEBUG)
        std::cout << prevToken << std::endl;
#endif
        auto& [valuePosition, tag, valueLength] = m_tokens[m_count];
        ++m_count;

        tag = convertToDecimal(m_tag, digits, tagPosition, digitCount);
        m_tag = 0;
        valuePosition = offset + tagPosition + digitCount + 1;
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
}

}