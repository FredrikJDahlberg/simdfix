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
        uint32_t tag;
        uint32_t valueOffset;
        uint32_t valueLength;
    };
private:
    Token m_tokens[128]{};
    size_t m_count = 0;
    uint32_t m_tag = 0;

    simd::Block m_data;

public:
    // produce a list of { | tag, offset, length, type | should be sorted by tag or group
    // order, body = tag, group = pos, data = len + data

    Tokenizer() = default;

    static void dump(size_t length, const uint8_t* buffer)
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

    void scan(const uint8_t* buffer, const size_t length)
    {
        m_count = 0;
        m_tag = 0;

        uint8_t digits[16];

        for (size_t offset = 0; offset + 15 < length; offset += 16)
        {
            m_data.put(buffer + offset, length - offset);

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
            const auto tagDigits = validTags.toUint64();  // 16 bytes to 4-bit nibble
            tags.get(0, digits); // read into GPR
            process(offset, tagDigits, digits);
        }
    }

    template<typename F>
    void forEach(F&& lambda) {
        for (int i = 0; i < m_count; ++i) {
            lambda(m_tokens[i]);
        }
    }

private:

    void process(const uint32_t offset, uint64_t digitsMap, const uint8_t* digits)
    {
        uint32_t bits = 0;
        // FIXME: calculate first tag outside loop?
        // FIXME: handle value lengths
        // FIXME: handle unterminated values
        for (int i = 0; i < 16; ++i)
        {
            std::printf("%02x ", digits[i]);
        }
        std::printf("\n");

        uint32_t position = 0;
        uint32_t digitCount = 0;
        while (digitsMap > 0)
        {
            const int trailingZeros = std::countr_zero(digitsMap);
            bits += trailingZeros;
            digitsMap >>= trailingZeros;
            const uint32_t digitBits = std::countr_one(digitsMap);
            position = bits / 4;
            digitCount = digitBits / 4;
            if (position + digitCount < 16)
            {
                auto& [tag, valueOffset, valueLength] = m_tokens[m_count];
                m_tag = 0;
                ++m_count;
                tag = convertToDecimal(m_tag, digits, position, digitCount);
                valueOffset = offset + position + digitCount + 1;
                valueLength = digitCount;
                std::printf("token, tag = %d, len = %d, pos = %d/%d\n",
                            tag, valueLength, valueOffset, position + digitCount);
                digitsMap >>= digitBits;
                bits += digitBits;
            }
            else
            {
                m_tag = digits[15];
                digitsMap >>= 4;
            }
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
            decimal = (decimal * 10) + *digit++;
        }
        return decimal;
    }
};
}

#endif //SIMD_FIX_TOKENIZER_H
