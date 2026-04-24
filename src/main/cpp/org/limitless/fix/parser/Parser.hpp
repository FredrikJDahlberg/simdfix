//
// Created by Fredrik Dahlberg on 2026-04-22.
//

#ifndef SIMD_FIX_PARSER_H
#define SIMD_FIX_PARSER_H

#include <expected>

#include "org/limitless/fix/parser/Tokenizer.hpp"
#include "org/limitless/fix/parser/BitSet64.hpp"
#include "org/limitless/fix/parser/Message.hpp"

namespace org::limitless::fix::parser {

class Parser
{
    static constexpr uint32_t BeginStringTag = 8;
    static constexpr uint32_t BodyLengthTag = 9;
    static constexpr uint32_t MessageTypeTag = 35;
    static constexpr uint32_t CheckSumTag = 10;

    static constexpr uint8_t FieldEnd = 0x01;
    static constexpr uint8_t BeginString[11] = { '8', '=', 'F', 'I', 'X', 'T', '.', '1', '.', '1', FieldEnd };

    Tokenizer m_tokenizer{};

public:

    enum class Status
    {
        InvalidBeginString,
        InvalidMessageTypeTag,
        InvalidCheckSumTag,
        InvalidBodyLengthTag,
        InvalidBodyLength,
        InvalidCheckSum,
        Success
    };

    template <typename Handler>
    // FIXME: restrictions on handler
    std::pair<size_t, Status> parse(const std::span<const uint8_t> buffer, const Handler handler)
    {
        m_present.set();
        auto [processed, checkSum] = m_tokenizer.scan(buffer);
        m_present >>= 64 - m_tokenizer.size();
        auto status = checkRequiredFields(buffer.data(), checkSum);
        if (status == Status::Success)
        {
            handler(nullptr);  // FIXME
        }
        return {processed, status};
    }

    const Tokenizer::Token* nextByTag(const uint16_t tag)
    {  // assume that fields are access once in tag order
        const uint32_t offset = m_present.zerosRight();
        auto tokens = m_tokenizer.begin();
        auto size = m_tokenizer.size();
        for (uint32_t position = offset; position < size; ++position)
        {
            if (tokens[position].tag == tag)
            {
                m_present.clear(position);
                return &tokens[position];
            }
        }
        return nullptr;
    }

    const Tokenizer::Token* nextByPos(uint32_t position)
    {  // assume that fields are access once in tag order
        if (m_tokenizer.size() >= position || m_present.get(position) == 0)
        {
            return nullptr;
        }
        m_present.clear(position);
        return m_tokenizer.begin() + position;
    }

private:

    BitSet64 m_present{}; // FIXME 128 bits

    Status checkRequiredFields(const uint8_t* buffer, uint8_t messageCheckSum) const;
};
}

#endif //SIMD_FIX_PARSER_H
