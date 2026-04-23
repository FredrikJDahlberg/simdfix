//
// Created by Fredrik Dahlberg on 2026-04-23.
//
#include "org/limitless/fix/parser/Parser.hpp"
#include "org/limitless/fix/parser/Utils.hpp"

namespace org::limitless::fix::parser {

size_t Parser::parse(std::span<const uint8_t> buffer, Error& error)
{
    uint8_t checkSum;
    const size_t processed = m_tokenizer.scan(buffer, checkSum);
    error = checkRequiredFields(buffer.data(), checkSum);
    return processed;
}

Parser::Error Parser::checkRequiredFields(const uint8_t* buffer, const uint8_t messageCheckSum) const
{
    if (std::memcmp(buffer, BeginString, sizeof(BeginString) - 1) != 0)
    {
        return Error::InvalidBeginString;
    }
    auto tokens = m_tokenizer.begin();
    const auto count = m_tokenizer.end() - tokens;
    const auto& messageType = tokens[2];
    if (messageType.tag != MessageTypeTag)
    {
        return Error::InvalidMessageTypeTag;
    }
    // FIXME: check message type value

    const auto& checkSum = tokens[count - 1];
    if (checkSum.tag != CheckSumTag)
    {
        return Error::InvalidCheckSumTag;
    }

    const auto& [position, tag, length] = tokens[1];
    if (tag != BodyLengthTag)
    {
        return Error::InvalidBodyLengthTag;
    }
    if (asciiToDecimal(buffer + position, length) != checkSum.position - messageType.position)
    {
        return Error::InvalidBodyLength;
    }
    if (asciiToDecimal(buffer + checkSum.position - 3, 3) != messageCheckSum)
    {
        return Error::InvalidCheckSum;
    }
    return Error::Success;
}
}
