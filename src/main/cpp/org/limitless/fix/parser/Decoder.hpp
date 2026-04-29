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
    struct Tags
    {
        static constexpr uint16_t BeginString = 8;
        static constexpr uint16_t BodyLength = 9;
        static constexpr uint16_t MessageType = 35;
        static constexpr uint16_t SenderCompID = 49;
        static constexpr uint16_t TargetCompID = 56;
        static constexpr uint16_t SendingTime = 52;
        static constexpr uint16_t CheckSum = 10;
    };

    struct Position
    {
        static constexpr uint32_t BeginString = 0;
        static constexpr uint32_t BodyLength = 1;
        static constexpr uint32_t MessageType = 2;
    };

    static constexpr uint8_t FieldEnd = 0x01;
    static constexpr uint8_t BeginString[11] = { '8', '=', 'F', 'I', 'X', 'T', '.', '1', '.', '1', FieldEnd };

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
        auto [processed, checkSum, status ] = m_tokenizer.scan(buffer);
        if (status != ParserStatus::Success)
        {
            return { processed, status };
        }

        const auto tokens = m_tokenizer.begin();
        const auto count = static_cast<int32_t>(m_tokenizer.size());
        auto result = checkRequiredFields(buffer.data(), checkSum);
        if (result == ParserStatus::Success)
        {
            result = handler.handle(buffer, std::span(tokens, count));
        }
        return { processed, result};
    }

private:
    ParserStatus checkRequiredFields(const uint8_t* buffer, uint8_t messageCheckSum) const;

};
}

#endif //SIMD_FIX_DECODER_HPP
