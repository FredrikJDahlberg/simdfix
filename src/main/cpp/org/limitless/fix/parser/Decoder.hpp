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
            result = handler.handle(buffer, std::span(tokens, count), m_tokenizer.tags());
        }
        return { processed, result};
    }

private:
    ParserStatus checkRequiredFields(const uint8_t* buffer, const uint8_t messageCheckSum) const
    {
        auto tokens = m_tokenizer.begin();
        const auto count = m_tokenizer.end() - tokens;
        const auto& messageType = tokens[Position::MessageType];
        if (messageType.tag != Tags::MessageType)
        {
            return ParserStatus::InvalidMessageTypeTag;
        }
        // message type is validated in the message handler

        const auto& checkSum = tokens[count - 1];
        const auto& [position, tag, length] = tokens[1];
        if (tag != Tags::BodyLength)
        {
            return ParserStatus::InvalidBodyLengthTag;
        }
        if (utils::asciiToDecimal(0, buffer + position, length) != checkSum.position - messageType.position)
        {
            return ParserStatus::InvalidBodyLength;
        }
        if (utils::asciiToDecimal(0, buffer + checkSum.position, 3) != messageCheckSum)
        {
            return ParserStatus::InvalidCheckSum;
        }

        for (int i = 0; i < count; ++i)
        {
            auto [position, tag, length] = tokens[i];
            std::printf("%3d tag = %3d, pos = %3d, len = %3d\n", i, tag, position, length);
        }
        // sender, target and message sequence number are validated in message decoder
        return ParserStatus::Success;
    }

};
}

#endif //SIMD_FIX_DECODER_HPP
