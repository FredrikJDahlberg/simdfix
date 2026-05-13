//
// Created by Fredrik Dahlberg on 2026-04-23.
//
#include "org/limitless/fix/parser/Decoder.hpp"
#include "org/limitless/fix/utils/Utils.hpp"

namespace org::limitless::fix::parser {

using namespace org::limitless::fix::utils;

ParserStatus Decoder::checkRequiredFields(const uint8_t* buffer, const uint8_t messageCheckSum) const
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
    if (asciiToDecimal(0, buffer + position, length) != checkSum.position - messageType.position)
    {
        return ParserStatus::InvalidBodyLength;
    }
    if (asciiToDecimal(0, buffer + checkSum.position, 3) != messageCheckSum)
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

}
