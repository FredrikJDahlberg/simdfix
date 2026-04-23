//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#ifndef SIMD_FIX_TOKENIZER_H
#define SIMD_FIX_TOKENIZER_H

#include <ostream>
#include <span>

#include "org/limitless/fix/parser/Uint8x16.hpp"

namespace org::limitless::fix::parser {

class Tokenizer
{
public:
    using position_t = uint32_t;
    using length_t = uint32_t;
    using data_t = uint8_t;

    static constexpr data_t TagEnd = '=';
    static constexpr data_t FieldEnd = '\x01';
    static constexpr data_t Zero = '0';
    static constexpr data_t Nine = '9';
    static constexpr data_t True = 255;
    static constexpr data_t False = 0;
    static constexpr data_t BeginString[11] = { '8', '=', 'F', 'I', 'X', 'T', '.', '1', '.', '1', Tokenizer::FieldEnd };

    static constexpr uint32_t BeginStringTag = 8;
    static constexpr uint32_t BodyLengthTag = 9;
    static constexpr uint32_t MsgTypeTag = 35;
    static constexpr uint32_t CheckSumTag = 10;

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

    Tokenizer() noexcept = default;
    ~Tokenizer() = default;

    Tokenizer(const Tokenizer&) = delete;
    Tokenizer& operator=(const Tokenizer&) = delete;
    Tokenizer(Tokenizer&&) = delete;
    Tokenizer& operator=(Tokenizer&&) = delete;

    size_t scan(const data_t* buffer, length_t length);

    [[nodiscard]] const Token* begin() const noexcept
    {
        return m_tokens + 2;
    }
    [[nodiscard]] const Token* end() const noexcept
    {
        return m_tokens + m_count - 1;
    }

    [[nodiscard]] std::span<const Token> tokens() const noexcept
    {
        return { m_tokens, m_count };
    }

private:
    Token m_tokens[128]{};
    size_t m_count{};
    uint16_t m_tag{};
    int32_t m_position{};
    simd::Uint8x16 m_data;

    bool processBlock(position_t offset, uint64_t tagDigitFlags, const data_t* digits, position_t nonTagBitPos);
    void checkRequiredFields(position_t offset, data_t checkSum, const data_t* buffer) const;
};
}

#endif //SIMD_FIX_TOKENIZER_H
