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
public:
    typedef uint32_t position_t;
    typedef uint32_t length_t;
    typedef uint8_t data_t;

    static constexpr data_t TagEnd = '=';
    static constexpr data_t FieldEnd = '\x01';
    static constexpr data_t Zero = '0';
    static constexpr data_t Nine = '9';
    static constexpr data_t True = 255;
    static constexpr data_t False = 0;
    static constexpr data_t BeginString[11] = { '8', '=', 'F', 'I', 'X', 'T', '.', '1', '.', '1', Tokenizer::FieldEnd };

    const simd::Uint8x16 TagEndsBlock{TagEnd};
    const simd::Uint8x16 FieldEndsBlock{FieldEnd};
    const simd::Uint8x16 ZerosBlock{Zero};
    const simd::Uint8x16 NinesBlock{Nine};
    const simd::Uint8x16 TrueBlock{True};

    struct Token
    {
        position_t position;
        uint16_t tag;
        uint16_t length;
    };

    Tokenizer() = default;

    size_t scan(const uint8_t* buffer, length_t length);

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

    static void dump(const length_t length, const data_t* buffer)
    {
        for (length_t i = 0; i < length; ++i)
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
        const auto data = reinterpret_cast<const data_t*>(&vector);
        for (position_t j = 0; j < 16; ++j)
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

    bool process(position_t offset, uint64_t tagDigitFlags, const data_t* digits, position_t nonTagBitPos);

    void checkRequiredFields(position_t offset, data_t checkSum, const data_t* buffer) const;

    template <data_t Base = 0>
    static uint32_t convertToDecimal(const uint32_t& value, const data_t* buffer, const position_t position, const length_t length)
    {
        uint32_t decimal = value;
        const data_t* digit = buffer + position;
        const data_t* end = digit + length;
        while (digit < end)
        {
            decimal = decimal * 10 + *digit++ - Base;
        }
        return decimal;
    }

    static uint32_t asciiToDecimal(const data_t* buffer, const position_t position, const length_t length)
    {
        return convertToDecimal<'0'>(0, buffer, position, length);
    }
};
}

#endif //SIMD_FIX_TOKENIZER_H
