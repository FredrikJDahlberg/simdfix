//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#ifndef SIMD_FIX_TOKENIZER_H
#define SIMD_FIX_TOKENIZER_H

#include <span>
#include <ostream>

#include "org/limitless/fix/parser/Token.hpp"
#include "org/limitless/fix/parser/Uint8x16.hpp"
#include "org/limitless/fix/parser/ParserStatus.hpp"

namespace org::limitless::fix::parser {

class Tokenizer
{
public:
    struct Result
    {
        Result(const uint32_t bytes, const uint32_t check, const ParserStatus error) :
            processed(static_cast<uint16_t>(bytes)), checkSum(static_cast<uint8_t>(check)), status(error)
        {
        }

        uint16_t processed;
        uint8_t checkSum;
        ParserStatus status;
    };
    using position_t = uint32_t;
    using length_t = uint16_t;
    using value_t = uint16_t;
    using data_t = uint8_t;

    static constexpr size_t MaxSize = 128;
    static constexpr data_t False = 0;
    static constexpr uint32_t CheckSumTag = 10;

    const simd::Uint8x16 TagEndsBlock{'='};
    const simd::Uint8x16 FieldEndsBlock{0x01};
    const simd::Uint8x16 ZerosBlock{'0'};
    const simd::Uint8x16 NinesBlock{'9'};
    const simd::Uint8x16 TrueBlock{0xff};

    Tokenizer() noexcept = default;
    ~Tokenizer() = default;

    Tokenizer(const Tokenizer&) = delete;
    Tokenizer& operator=(const Tokenizer&) = delete;
    Tokenizer(Tokenizer&&) = delete;
    Tokenizer& operator=(Tokenizer&&) = delete;

    Result scan(std::span<const data_t> buffer);

    [[nodiscard]] Token* begin() noexcept
    {
        return m_tokens;
    }

    [[nodiscard]] Token* end() noexcept
    {
        return m_tokens + m_count;
    }

    [[nodiscard]] std::span<Token> tokens() noexcept
    {
        return { m_tokens, m_count };
    }

    [[nodiscard]] const Token* begin() const noexcept
    {
        return m_tokens;
    }

    [[nodiscard]] const Token* end() const noexcept
    {
        return m_tokens + m_count;
    }

    [[nodiscard]] std::span<const Token> tokens() const noexcept
    {
        return { m_tokens, m_count };
    }

    [[nodiscard]] size_t size() const noexcept
    {
        return m_count;
    }

private:
    Token m_tokens[MaxSize]{};

    size_t m_count{};
    uint32_t m_tag{};
    int32_t m_position{};
    simd::Uint8x16 m_data{};

    bool processBlock(position_t offset, uint64_t tagDigitFlags, const data_t* digits, position_t nonTagBitPos);
};
}

#endif //SIMD_FIX_TOKENIZER_H
