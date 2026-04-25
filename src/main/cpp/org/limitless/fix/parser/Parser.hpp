//
// Created by Fredrik Dahlberg on 2026-04-22.
//

#ifndef SIMD_FIX_PARSER_HPP
#define SIMD_FIX_PARSER_HPP

#include <expected>

#include "org/limitless/fix/parser/Tokenizer.hpp"
#include "org/limitless/fix/parser/BitSet64.hpp"
#include "org/limitless/fix/parser/Message.hpp"
#include "org/limitless/fix/parser/ParserStatus.hpp"

#include "org/limitless/fix/messages/Logon.hpp"

namespace org::limitless::fix::parser {

class Parser
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

    static constexpr uint8_t FieldEnd = 0x01;
    static constexpr uint8_t BeginString[11] = { '8', '=', 'F', 'I', 'X', 'T', '.', '1', '.', '1', FieldEnd };

    Tokenizer m_tokenizer{};
    BitSet64 m_present{}; // FIXME 128 bits

public:

    template <typename Handler>
    // FIXME: restrictions on handler
    std::pair<size_t, ParserStatus> parse(const std::span<const uint8_t> buffer, const Handler handler)
    {
        m_present.set();
        auto [processed, checkSum] = m_tokenizer.scan(buffer);
        const auto tokens = m_tokenizer.begin();
        const auto count = static_cast<int32_t>(m_tokenizer.size());
        m_present >>= 64 - count;
        auto status = checkRequiredFields(buffer.data(), checkSum);
        m_present.clear(0).clear(1).clear(2).clear(count - 1);
        if (status == ParserStatus::Success)
        {
            Message message{buffer, std::span(tokens, count), m_present};
            handler(&message);
        }
        return {processed, status};
    }

private:
    ParserStatus checkRequiredFields(const uint8_t* buffer, uint8_t messageCheckSum) const;

};
}

#endif //SIMD_FIX_PARSER_HPP
