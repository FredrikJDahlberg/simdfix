//
// Created by Fredrik Dahlberg on 2026-04-22.
//

#ifndef SIMD_FIX_PARSER_H
#define SIMD_FIX_PARSER_H

#include "org/limitless/fix/parser/Tokenizer.hpp"

namespace org::limitless::fix::parser {

class Parser
{
    static constexpr uint32_t BeginStringTag = 8;
    static constexpr uint32_t BodyLengthTag = 9;
    static constexpr uint32_t MsgTypeTag = 35;
    static constexpr uint32_t CheckSumTag = 10;
    static constexpr uint8_t BeginString[11] = { '8', '=', 'F', 'I', 'X', 'T', '.', '1', '.', '1', Tokenizer::FieldEnd };

    Tokenizer m_tokenizer{};

public:

    void parse(std::span<const uint8_t> buffer);

private:

   void checkRequiredFields(const uint8_t* buffer, uint8_t messageCheckSum) const;
};
}

#endif //SIMD_FIX_PARSER_H
