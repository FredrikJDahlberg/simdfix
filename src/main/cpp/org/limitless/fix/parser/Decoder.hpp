//
// Created by Fredrik Dahlberg on 2026-04-22.
//

#ifndef SIMD_FIX_DECODER_HPP
#define SIMD_FIX_DECODER_HPP

#include "org/limitless/fix/parser/Tokenizer.hpp"
#include "org/limitless/fix/parser/MessageDecoder.hpp"
#include "org/limitless/fix/parser/ParserStatus.hpp"

namespace org::limitless::fix::parser {

class Decoder
{
    Tokenizer m_tokenizer{};

public:
    struct Result
    {
        size_t processed;
        ParserStatus status;
    };
    Decoder() = default;

    template <typename Handler>
    Result parse(const std::span<const uint8_t> buffer, Handler& handler)
    {
#if 0
        const auto scan = m_tokenizer.parse(buffer);
        if (scan.status != ParserStatus::Success)
        {
            return { scan.processed, scan.status };
        }
        const auto count = static_cast<int32_t>(m_tokenizer.size());
        return { scan.processed, handler.handle(buffer, std::span(m_tokenizer.begin(), count), m_tokenizer.tags()) };
#endif
        return { 0, ParserStatus::InvalidCheckSum} ;
    }
};
}

#endif //SIMD_FIX_DECODER_HPP
