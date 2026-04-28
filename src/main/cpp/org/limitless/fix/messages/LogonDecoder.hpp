//
// Created by Fredrik Dahlberg on 2026-04-25.
//
// Will be generated
//

#ifndef SIMD_FIX_LOGON_DECODER_HPP
#define SIMD_FIX_LOGON_DECODER_HPP

#include <expected>

#include "org/limitless/fix/parser/MessageDecoder.hpp"
#include "org/limitless/fix/parser/GroupDecoder.hpp"
#include "org/limitless/fix/parser/ParserStatus.hpp"

namespace org::limitless::fix::generated {

using namespace org::limitless::fix::parser;

struct LogonGrammar
{
    static constexpr std::array<Entry<Dictionary>, 9> Meta {
        {
            {1, {1, 0, false} },
            {10, {10, 0, true}},
            {34, {34, 0, true}},
            {49, {49, 12, true}},
            {102, {102, 24, true}},
            {627, {627, 10, false}},
            {628, {628, 10, false}},
            {629, {629, 10, false}},
            {630, {630, 10, false}}
        }};
};

struct LogonDecoder : MessageDecoder<LogonGrammar>
{
    using Message = MessageDecoder;
    using Group = GroupDecoder<LogonDecoder>;
    using Token = Tokenizer::Token;

    static constexpr uint16_t MessageId = 'A';
    static constexpr size_t MemberCount = 3;

    LogonDecoder() = default;
#if 0
    struct HopGroupDecoder : Group
    {
        using Group = GroupDecoder<Grammar>;

        static constexpr uint16_t Type = 10;

        explicit HopGroupDecoder(const Message* message) : Group(message)
        {
        }

        HopGroupDecoder& wrap() // MessageDecoder<Grammar>& message
        {
            // base_t::m_message = message;
            // base_t::m_message->m_present;
            // FIXME: pointing to wrong m_message token table is empty
            // Group::check(Group::m_message->next(627));
            return *this;
        }

        HopGroupDecoder& clear()
        {
            Message::clear();
            return *this;
        }

        [[nodiscard]] std::expected<uint32_t, ParserStatus> count() const
        {
            return Group::count();
        }

        HopGroupDecoder& next()
        { // move to next delimiter
            //Message::next();
            return *this;
        }

        [[nodiscard]] std::expected<uint32_t, ParserStatus> hopCompID() const
        {
            // using wrong presence map?
            return 0;
            // return Group::m_message->template getUnsigned<628>(Group::m_delim == 628);
        }

        [[nodiscard]] std::expected<uint32_t, ParserStatus> hopRefID() const
        {
            return 0;
            // return Group::m_message->template getUnsigned<629>(Group::m_delim == 629);
        }
    };

    HopGroupDecoder m_hopGroup{this};
#endif
    LogonDecoder& wrap(std::span<const uint8_t> data, const std::span<Token> tokens)
    {
        Message::wrap(data, tokens);
        // m_hopGroup.wrap();
        return *this;
    }

    std::expected<std::span<const uint8_t>, ParserStatus> sender()
    {
        return this->template getString<56>(true);
    }

    std::expected<std::span<const uint8_t>, ParserStatus> target()
    {
        return this->template getString<49>(true);
    }

    std::expected<uint32_t, ParserStatus> expectedSeqNum()
    {
        return this->template getUnsigned<34>(true);
    }

    std::expected<std::span<const uint8_t>, ParserStatus> onBehalfOfCompID()
    {
        return this->template getString<115>(false);
    }

    std::expected<uint32_t, ParserStatus> nextExpectedSeqNum()
    {
        return this->template getUnsigned<789>(false);
    }
#if 0
    HopGroupDecoder& hopGroup()
    {
        // return m_hopGroup.wrap(this);
        return m_hopGroup.wrap();
    }
#endif
};
}

#endif //SIMD_FIX_LOGON_DECODER_HPP
