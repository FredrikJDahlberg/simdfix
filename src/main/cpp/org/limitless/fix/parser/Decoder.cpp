//
// Created by Fredrik Dahlberg on 2026-04-23.
//
#include "org/limitless/fix/parser/Decoder.hpp"
#include "org/limitless/fix/parser/Utils.hpp"

namespace org::limitless::fix::parser {

ParserStatus Decoder::checkRequiredFields(const uint8_t* buffer, const uint8_t messageCheckSum) const
{
    if (std::memcmp(buffer, BeginString, sizeof(BeginString) - 1) != 0)
    {
        return ParserStatus::InvalidBeginString;
    }

    // FIXME: check minimum number of required tokens

    auto tokens = m_tokenizer.begin();
    const auto count = m_tokenizer.end() - tokens;
    const auto& messageType = tokens[2];
    if (messageType.tag != Tags::MessageType)
    {
        return ParserStatus::InvalidMessageTypeTag;
    }
    // FIXME: check message type value

    const auto& checkSum = tokens[count - 1];
    if (checkSum.tag != Tags::CheckSum)
    {
        return ParserStatus::InvalidCheckSumTag;
    }
    const auto& [position, tag, length] = tokens[1];
    if (tag != Tags::BodyLength)
    {
        return ParserStatus::InvalidBodyLengthTag;
    }
    if (asciiToDecimal(buffer + position, length) != static_cast<uint32_t>(checkSum.position - messageType.position))
    {
        return ParserStatus::InvalidBodyLength;
    }
    if (asciiToDecimal(buffer + checkSum.position, 3) != messageCheckSum)
    {
        return ParserStatus::InvalidCheckSum;
    }

    // FIXME:
    //static constexpr uint32_t SenderCompID = 49;
    //static constexpr uint32_t TargetCompID = 56;

    return ParserStatus::Success;
}
}
