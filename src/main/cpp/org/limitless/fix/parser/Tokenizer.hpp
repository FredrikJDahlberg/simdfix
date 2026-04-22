//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#ifndef SIMD_FIX_TOKENIZER_H
#define SIMD_FIX_TOKENIZER_H

#include <ostream>

#include "org/limitless/fix/parser/Uint8x16.hpp"

namespace org::limitless::fix::parser {

class Tokenizer
{
    static constexpr char TagEnd = '=';
    static constexpr char FieldEnd = '\x01';
    static constexpr char Zero = '0';
    static constexpr char Nine = '9';
    static constexpr uint8_t True = 255;
    static constexpr uint8_t False = 0;

    const simd::Uint8x16 TagEndsBlock{TagEnd};
    const simd::Uint8x16 FieldEndsBlock{FieldEnd};
    const simd::Uint8x16 ZerosBlock{Zero};
    const simd::Uint8x16 NinesBlock{Nine};
    const simd::Uint8x16 TrueBlock{True};

public:
    struct Token
    {
        uint32_t position;
        uint16_t tag;
        uint16_t length;
    };
    friend std::ostream& operator<<(std::ostream& os, const Token& obj)
    {
        return os << "{Token, tag = " << obj.tag << ", valueOffset = " << obj.position << ", valueLength = " << obj.length << "}";
    }

    Tokenizer() = default;

    size_t scan(const uint8_t* buffer, int32_t length);

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

    static void dump(const uint32_t length, const uint8_t* buffer)
    {
        for (uint32_t i = 0; i < length; ++i)
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
    size_t m_count;
    uint16_t m_tag{};
    int32_t m_position{};
    simd::Uint8x16 m_data;

    bool process(int32_t offset, uint64_t tagDigitFlags, const uint8_t* digits, uint32_t nonTagBitPos);

    template <uint8_t Base = 0>
    static uint32_t convertToDecimal(const uint32_t& value, const uint8_t* buffer, const int32_t position, const int32_t length)
    {
        uint32_t decimal = value;
        const uint8_t* digit = buffer + position;
        const uint8_t* end = digit + length;
        while (digit < end)
        {
            decimal = decimal * 10 + *digit++ - Base;
        }
        return decimal;
    }

    static uint32_t asciiToDecimal(const uint8_t* buffer, const int32_t position, const int32_t length)
    {
        return convertToDecimal<'0'>(0, buffer, position, length);
    }
};
}

#endif //SIMD_FIX_TOKENIZER_H
