//
// Created by Fredrik Dahlberg on 2026-04-25.
//
// Will be generated
//

#ifndef SIMD_FIX_LOGON_HPP
#define SIMD_FIX_LOGON_HPP

#include <exception>

#include "org/limitless/fix/parser/Message.hpp"
#include "org/limitless/fix/messages/Dictionary.hpp"

namespace org::limitless::fix::messages {

using namespace org::limitless::fix::parser;

struct Logon : parser::Message
{
    Logon(const std::span<const uint8_t> data, std::span<Token> tokens, parser::BitSet64 present)
        : Message(data, tokens, present)
    {
    }

    std::expected<std::span<const uint8_t>, ParserStatus> sender()
    {
        const auto token = nextByTag(56);
        if (token == nullptr)
        {
            return std::unexpected{ParserStatus::RequiredFieldMissing};
        }
        return m_data.subspan(token->position, token->length);
    }

    std::expected<std::span<const uint8_t>, ParserStatus> target()
    {
        const auto token = nextByTag(49);
        if (token == nullptr)
        {
            return std::unexpected{ParserStatus::RequiredFieldMissing};
        }
        return m_data.subspan(token->position, token->length);
    }

    uint32_t expectedSeqNum()
    {
        const auto token = nextByTag(34);
        if (token == nullptr)
        {
            throw std::invalid_argument{"required field missing, tag = 49"};
        }
        return convertToUnsigned(token);
    }

    std::span<const uint8_t> onBehalfOfCompID()
    {
        const auto token = nextByTag(115);
        if (token == nullptr)
        {
            return NullSpan;
        }
        return m_data.subspan(token->position, token->length);
    }

    std::pair<bool, uint32_t> nextExpectedSeqNum()
    {
        const auto token = nextByTag(789);
        uint32_t value = 0;
        if (token != nullptr)
        {
            value = convertToUnsigned(token);
        }
        return {token != nullptr, value};
    }


    struct HopGroup
    {
        static constexpr std::array<const TokenMeta*, 3> meta =
        {
            dictionary(628), dictionary(629), dictionary(630)
        };

        Message& m_message;
        Token* m_count;
        Token* m_first{};
        int32_t position{};

        explicit HopGroup(Message& message) : m_message(message)
        {
            m_count = const_cast<Token*>(message.nextByTag(627));
            // m_first = message.nextByPosiiton();
        }

        int32_t count()
        {
            if (m_count == nullptr)
            {
                return 0;
            }
            // identify group members
            m_first = m_count + 1;
            int32_t found = 0;
            // FIXME: check type of next token
            return m_count->position;
        }

        HopGroup& next()
        {
            m_first = m_message.nextByTag(m_first->tag);
            return *this;
        }

        bool hasNext() const
        {
            return m_first != nullptr;
        }

        std::expected<int32_t, ParserStatus> hopCompID() const
        {
            const auto token = m_message.nextByTag(628);
            uint32_t value = 0;
            if (token != nullptr)
            {
                value = m_message.convertToUnsigned(token);
            }
            else
            {
                if (m_first->tag == 628)
                {
                    return value; // FIXME
                }
            }
            return value;
        }

        std::pair<bool, uint32_t> hopRefID()         // FIXME: optional unless first in group
        {
            const auto token = m_message.nextByTag(629);
            uint32_t value = 0;
            if (token != nullptr)
            {
                value = m_message.convertToUnsigned(token);
            }
            return {token != nullptr, value};
        }
    };

    HopGroup hopGroup()
    {
        return HopGroup{*this};
    }
};
}

#endif //SIMD_FIX_LOGON_HPP
