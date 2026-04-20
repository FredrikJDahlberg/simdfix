//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#ifndef SIMD_FIX_TOKENIZER_H
#define SIMD_FIX_TOKENIZER_H

#include <bit>
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
    friend std::ostream& operator<<(std::ostream& os, const Token& obj)
    {
        return os << "{Token, tag = " << obj.tag << ", valueOffset = " << obj.valueOffset << ", valueLength = " << obj.valueLength << "}";
    }

    Tokenizer() = default;

    void scan(const uint8_t* buffer, const int32_t length)
    {
        m_count = 0;
        m_tag = 0;

        uint8_t digits[16];
        m_tokens[0] = { 8, 2, 6 };
        // std::printf("token, tag = %d, len = %d, pos = %d\n", m_tokens->tag, m_tokens->valueLength, m_tokens->valueOffset);
        m_count = 1;

        uint32_t bits = 4;
        uint64_t checkSum = 0;
        for (int32_t offset = 0; offset + 15 < length; offset += 16)
        {
            m_data.put(buffer + offset, length - offset);
            checkSum += m_data.sum();

            // dump(16, buffer + offset);
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
        // FIXME: adjust checksum and stop tokenizer after tag 10
        // std::printf("%3d\n", checkSum % 256);
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

    void process(const int32_t offset, const uint64_t tagDigitFlags, const uint8_t* digits, uint32_t nonTagBitPos)
    {
        /*
        for (int i = 0; i < 16; ++i)
        {
            std::printf("%02x ", digits[i]);
        }
        std::printf("\n");
        */
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
            // std::cout << prevToken << std::endl;

            auto& [tag, valuePosition, valueLength] = m_tokens[m_count];
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

    static uint32_t convertToDecimal(const int32_t& value,
                                     const uint8_t* buffer,
                                     const int32_t position,
                                     const int32_t length)
    {
        int32_t decimal = value;
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
