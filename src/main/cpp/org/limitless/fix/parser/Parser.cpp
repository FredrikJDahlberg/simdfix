//
// Created by Fredrik Dahlberg on 2026-04-23.
//
#include "org/limitless/fix/parser/Parser.hpp"
#include "org/limitless/fix/parser/Utils.hpp"

namespace org::limitless::fix::parser {

Parser::Status Parser::checkRequiredFields(const uint8_t* buffer, const uint8_t messageCheckSum) const
{
    if (std::memcmp(buffer, BeginString, sizeof(BeginString) - 1) != 0)
    {
        return Status::InvalidBeginString;
    }
    auto tokens = m_tokenizer.begin();
    const auto count = m_tokenizer.end() - tokens;
    const auto& messageType = tokens[2];
    if (messageType.tag != MessageTypeTag)
    {
        return Status::InvalidMessageTypeTag;
    }
    // FIXME: check message type value

    const auto& checkSum = tokens[count - 1];
    if (checkSum.tag != CheckSumTag)
    {
        return Status::InvalidCheckSumTag;
    }

    const auto& [position, tag, length] = tokens[1];
    if (tag != BodyLengthTag)
    {
        return Status::InvalidBodyLengthTag;
    }
    if (asciiToDecimal(buffer + position, length) != static_cast<uint32_t>(checkSum.position - messageType.position))
    {
        return Status::InvalidBodyLength;
    }
    if (asciiToDecimal(buffer + checkSum.position, 3) != messageCheckSum)
    {
        return Status::InvalidCheckSum;
    }
    return Status::Success;
}
}
