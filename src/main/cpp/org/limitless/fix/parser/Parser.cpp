//
// Created by Fredrik Dahlberg on 2026-04-23.
//
#include "org/limitless/fix/parser/Parser.hpp"
#include "org/limitless/fix/parser/Utils.hpp"

namespace org::limitless::fix::parser {

void Parser::parse(std::span<const uint8_t> buffer)
{
    uint8_t checkSum;
    size_t processed = m_tokenizer.scan(buffer, checkSum);
    checkRequiredFields(buffer.data(), checkSum);
}

void Parser::checkRequiredFields(const uint8_t* buffer, const uint8_t messageCheckSum) const
{
    if (std::memcmp(buffer, BeginString, sizeof(BeginString) - 1) != 0)
    {
        throw std::invalid_argument("invalid begin string");
    }
    auto tokens = m_tokenizer.begin();
    const auto count = m_tokenizer.end() - tokens;
    const auto& bodyLength = tokens[2];
    if (bodyLength.tag != MsgTypeTag)
    {
        throw std::invalid_argument("invalid message type tag");
    }

    const auto& checkSum = tokens[count - 1];
    if (checkSum.tag != CheckSumTag)
    {
        throw std::invalid_argument("invalid check sum tag");
    }
    const auto& [position, tag, length] = tokens[1];
    if (tag != BodyLengthTag)
    {
        throw std::invalid_argument("invalid body length tag");
    }
    if (asciiToDecimal(buffer + position, length) != checkSum.position - bodyLength.position)
    {
        throw std::invalid_argument("invalid body length");
    }
    if (asciiToDecimal(buffer + checkSum.position - 3, 3) != messageCheckSum)
    {
        throw std::invalid_argument("invalid checksum");
    }
}
}
