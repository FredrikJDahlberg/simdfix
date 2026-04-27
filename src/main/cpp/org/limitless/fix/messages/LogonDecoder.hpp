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

struct LogonDecoder : MessageDecoder
{
    static constexpr uint16_t MessageId = 1;

    static constexpr size_t MemberCount = 3;

    /*
    LogonDecoder(const std::span<const uint8_t> data, std::span<Token> tokens, BitSet64 present)
        : MessageDecoder(data, tokens, present)
    {
    }
    */

    LogonDecoder& wrap() // const std::span<Token> tokens,
    {
        m_hopGroup.wrap(*this);
        return *this;
    }

    std::expected<std::span<const uint8_t>, ParserStatus> sender()
    {
        return getString<56>(true);
    }

    std::expected<std::span<const uint8_t>, ParserStatus> target()
    {
        return getString<49>(true);
    }

    std::expected<uint32_t, ParserStatus> expectedSeqNum()
    {
        return getUnsigned<34>(true);
    }

    std::expected<std::span<const uint8_t>, ParserStatus> onBehalfOfCompID()
    {
        return getString<115>(false);
    }

    std::expected<uint32_t, ParserStatus> nextExpectedSeqNum()
    {
        return getUnsigned<789>(false);
    }

    struct HopGroupDecoder : GroupDecoder
    {
        static constexpr uint16_t Type = 10;

        HopGroupDecoder& wrap(MessageDecoder& message)
        {
            m_message = &message;
            auto groupCount = m_message->next(627);
            if (groupCount != nullptr)
            {
                m_count = m_message->convertToUnsigned(groupCount);
                m_offset = static_cast<int32_t>(groupCount - &message.m_tokens[0]);
                m_position = 0;
                std::printf("HOPP = %lld\n", m_offset);
            }

            return *this;
        }

        HopGroupDecoder& clear()
        {
            m_count = 0;
            m_position = 0;
            return *this;
        }

        [[nodiscard]] std::expected<uint32_t, ParserStatus> count() const
        {
            if (m_groupCount == nullptr)
            {
                return 0;
            }
            return m_count;
        }

        [[nodiscard]] bool hasNext() const
        {
            return m_position < m_count;
        }

        HopGroupDecoder& next()
        { // move to next delimiter
            ++m_offset;

            //m_group = m_message->next(m_offset + m_position);
            ++m_group;
            ++m_position;

            auto hepp = m_group - &m_message->m_tokens[0];
            std::printf("HEPP = %ld\n", hepp);
            return *this;
        }

        [[nodiscard]] std::expected<uint32_t, ParserStatus> hopCompID() const
        {
            return m_message->getUnsigned<628>(m_delim == 628);
        }

        [[nodiscard]] std::expected<uint32_t, ParserStatus> hopRefID() const
        {
            return m_message->getUnsigned<629>(m_delim == 629);
        }
    };

    HopGroupDecoder m_hopGroup{this};

    HopGroupDecoder& hopGroup()
    {
        return m_hopGroup;
    }
};
}

#endif //SIMD_FIX_LOGON_DECODER_HPP
