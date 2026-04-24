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
    using position_t = int32_t;
    using length_t = int32_t;
    using data_t = uint8_t;

    static constexpr data_t False = 0;

    const simd::Uint8x16 TagEndsBlock{'='};
    const simd::Uint8x16 FieldEndsBlock{0x01};
    const simd::Uint8x16 ZerosBlock{'0'};
    const simd::Uint8x16 NinesBlock{'9'};
    const simd::Uint8x16 TrueBlock{0xff};

    struct Token
    {
        uint32_t position;
        uint32_t tag;
        int32_t length;
    };

    Tokenizer() noexcept = default;
    ~Tokenizer() = default;

    Tokenizer(const Tokenizer&) = delete;
    Tokenizer& operator=(const Tokenizer&) = delete;
    Tokenizer(Tokenizer&&) = delete;
    Tokenizer& operator=(Tokenizer&&) = delete;

    [[nodiscard]] std::pair<uint16_t, uint8_t> scan(std::span<const data_t> buffer); // processed and checksum

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
};
}

#endif //SIMD_FIX_TOKENIZER_H
