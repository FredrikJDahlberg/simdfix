//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#ifndef SIMD_FIX_TOKENIZER_H
#define SIMD_FIX_TOKENIZER_H

#include <ostream>

#include "org/limitless/fix/parser/Block.hpp"

namespace org::limitless::fix::parser {

class Tokenizer
{
    static constexpr char TagEnd = '=';
    static constexpr char FieldEnd = '\x01';
    static constexpr char Zero = '0';
    static constexpr char Nine = '9';
    static constexpr uint8_t True = 255;
    static constexpr uint8_t False = 0;

    const simd::Block TagEndsBlock{TagEnd};
    const simd::Block FieldEndsBlock{FieldEnd};
    const simd::Block ZerosBlock{Zero};
    const simd::Block NinesBlock{Nine};
    const simd::Block TrueBlock{True};

public:
    struct Token
    {
        int32_t tag;
        int32_t valueOffset;
        int32_t valueLength;
    };

    Tokenizer() = default;

    void scan(const uint8_t* buffer, const size_t length)
    {
        m_count = 0;
        m_tag = 0;

        uint8_t digits[16];
        m_tokens[0] = { 8, 2, 6 };
        std::printf("token, tag = %d, len = %d, pos = %d\n", m_tokens->tag, m_tokens->valueLength, m_tokens->valueOffset);
        m_count = 1;

        uint32_t bits = 4;
        for (size_t offset = 0; offset + 15 < length; offset += 16)
        {
            m_data.put(buffer + offset, length - offset);
            dump(16, buffer + offset);
            // A digit is valid if followed by '=' or a validated digit
            const simd::Block digitFlags{m_data >= ZerosBlock & m_data <= NinesBlock};
            const simd::Block tagEnds{m_data == TagEndsBlock};
            simd::Block after{digitFlags & tagEnds.shiftLeft<1>()};
            after |= digitFlags & after.shiftLeft<1>();
            after |= digitFlags & after.shiftLeft<1>();
            after |= digitFlags & after.shiftLeft<1>();
            // after |= digitFlags & after.shiftLeft<1>();

            // A digit is valid if preceded by 0x01 or a validated digit
            simd::Block fieldEnds{m_data == FieldEndsBlock};
            simd::Block before = digitFlags & fieldEnds.shiftRight<1>();
            before |= digitFlags & before.shiftRight<1>();
            before |= digitFlags & before.shiftRight<1>();
            before |= digitFlags & before.shiftRight<1>();
            // before |= digitFlags & before.shiftRight<1>();
            const simd::Block validTags{after | before};
            const simd::Block tags{validTags.whenTrue(m_data - ZerosBlock)};
            auto tagDigits = validTags.toUint64();  // 16 bytes to 4-bit nibble
            tags.get(0, digits); // read into GPR
            tagDigits >>= bits;
            process(offset, tagDigits, digits, bits);
            bits = 0;
        }
    }

    Token* begin()
    {
        return m_tokens;
    }
    Token* end()
    {
        return m_tokens + m_count;
    }
    [[nodiscard]] const Token* begin() const
    {
        return m_tokens;
    }
    [[nodiscard]] const Token* end() const
    {
        return m_tokens + m_count;
    }

    static void dump(const size_t length, const uint8_t* buffer)
    {
        for (int i = 0; i < length; ++i)
        {
            if (const auto ch = buffer[i]; std::isprint(ch))
            {
                std::printf("%2c ", ch);
            }
            else
            {
                std::printf("%2c ", ch == 1 ? '|' : '?');
            }
        }
        std::printf("\n");
    }

    static void dump(const uint8x16_t& vector)
    {
        const auto data = reinterpret_cast<const uint8_t*>(&vector);
        for (int j = 0; j < 16; ++j)
        {
            std::printf("%02x ", data[j]);
        }
        std::printf("\n");
    }

private:
    Token m_tokens[128]{};
    size_t m_count = 0;

    int32_t m_tag = 0;
    int32_t m_position = 0;

    simd::Block m_data;

    void process(const int32_t offset, const uint64_t digitsMap, const uint8_t* digits, int32_t bits)
    {
        // FIXME: calculate first tag outside loop?
        // FIXME: handle value lengths
        // FIXME: handle unterminated values
        for (int i = 0; i < 16; ++i)
        {
            std::printf("%02x ", digits[i]);
        }
        std::printf("\n");

        const uint64_t lastDigitMap = digitsMap >> 60 & 0xF;
        // FIXME: handle split tags with more than one digit
        uint64_t remainingMap = digitsMap & 0x0FFFFFFFFFFFFFFFULL;
        int32_t tagPosition = 0;
        int32_t digitCount = 0;
        while (remainingMap > 0) {
            const auto trailingZeros = std::countr_zero(remainingMap);
            bits += trailingZeros;
            remainingMap >>= trailingZeros;
            const auto digitBits = std::countr_one(remainingMap);
            digitCount = digitBits / 4;
            tagPosition = bits / 4; //  + m_position;
            auto& prevToken = m_tokens[m_count - 1];
            prevToken.valueLength = offset + tagPosition - prevToken.valueOffset - 1 + m_position;
            std::printf("TAG = %d len = %d offset = %d, pos = %d POS= %d \n",
                prevToken.tag, prevToken.valueLength, prevToken.valueOffset, offset + tagPosition, m_position);

            auto& [tag, valueOffset, valueLength] = m_tokens[m_count++];
            tag = convertToDecimal(m_tag, digits, tagPosition, digitCount);
            m_tag = 0;
            m_position = 0;
            valueOffset = offset + tagPosition + digitCount + 1;
            remainingMap >>= digitBits;
            bits += digitBits;
        }
        if (lastDigitMap == 0xF)
        {
            m_tokens[m_count - 1].valueLength = 0;
            m_tag = digits[15];  // store split tags
            m_position = -digitCount + 1;
        }
    }

    static uint32_t convertToDecimal(const uint32_t& value,
                                     const uint8_t* buffer,
                                     const uint32_t position,
                                     const uint32_t length)
    {
        uint32_t decimal = value;
        const uint8_t* digit = buffer + position;
        const uint8_t* end = digit + length;
        while (digit < end)
        {
            decimal = decimal * 10 + *digit++;
        }
        return decimal;
    }
};
}

#endif //SIMD_FIX_TOKENIZER_H
