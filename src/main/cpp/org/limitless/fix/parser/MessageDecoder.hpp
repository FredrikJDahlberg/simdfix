//
// Created by Fredrik Dahlberg on 2026-04-24.
//

#ifndef SIMD_FIX_MESSAGE_DECODER_HPP
#define SIMD_FIX_MESSAGE_DECODER_HPP

#include <span>

#include <array>
#include "org/limitless/fix/parser/Dictionary.hpp"

#include "org/limitless/fix/parser/ParserStatus.hpp"
#include "org/limitless/fix/parser/Tokenizer.hpp"
#include "org/limitless/fix/parser/Utils.hpp"
#include "org/limitless/fix/parser/BitSet64.hpp"
#include "org/limitless/fix/parser/PerfectHashMap.hpp"

namespace org::limitless::fix::parser {

template <typename Grammar>
struct MessageDecoder
{
    using Token = Tokenizer::Token;
    using Message = MessageDecoder<Grammar>;

    std::span<const uint8_t> m_data{};
    std::span<Token> m_tokens{};
    BitSet64 m_present{};

    // FIXME: nested groups stack

    static constexpr std::array<Entry<Dictionary>, 9> LocalMeta = Grammar::Meta;
    static constexpr auto MessageGrammar = PerfectHashMap(
        std::span<const Entry<Dictionary>, LocalMeta.size()>(LocalMeta)
    );

    MessageDecoder() = default;

    explicit MessageDecoder(const std::span<Token> tokens)
        : m_tokens{tokens}
    {
    }

    MessageDecoder(const std::span<const uint8_t> data, const std::span<Token> tokens)
        : m_data{data}, m_tokens{tokens}
    {
    }

    void wrap(const std::span<const uint8_t> data, const std::span<Token> tokens)
    {
        m_data = data;
        m_tokens = tokens;
        m_present.set();
        const auto size = tokens.size();
        m_present >>= m_present.capacity() - size;
        m_present.clear(0).clear(1).clear(2).clear(size - 1); // already checked
    }

    [[nodiscard]] uint8_t type() const noexcept
    {
        return m_data[m_tokens[2].position]; // FIXME
    }

    template <int32_t Tag>
    std::expected<std::span<const uint8_t>, ParserStatus> getString(const bool required)
    {
        const auto token = next(Tag);
        if (token == nullptr && required)
        {
            return std::unexpected{ParserStatus::RequiredFieldMissing};
        }
        return m_data.subspan(token->position, token->length);
    }

    template <int32_t Tag>
    std::expected<uint32_t, ParserStatus> getUnsigned(const bool required)
    {
        const auto token = next(Tag);
        if (token == nullptr && required)
        {
            return std::unexpected(ParserStatus::RequiredFieldMissing);
        }
        return convertToUnsigned(token);
    }

    Tokenizer::Token* next(const int32_t tag)
    {  // assume that fields are access once in tag order
        const auto tokens = m_tokens;
        BitSet64 present{m_present};
        while (!present.empty())
        {
            const int32_t position = present.zerosRight();
            if (tokens[position].tag == tag)
            {
                m_present.clear(position);
                return &tokens[position];
            }
            present.clear(position);
        }
        return nullptr;
    }

#if 0
    Tokenizer::Token* next(const int32_t position)
    {  // assume that fields are access once in tag order
        // m_position should reside here
        if (m_tokens.size() >= position || m_present.get(position) == 0)
        {
            return nullptr;
        }
        m_present.clear(position);
        return &m_tokens[position];
    }
#endif
    uint32_t convertToUnsigned(const Token* token) const
    {
        return asciiToDecimal(m_data.data() + token->position, token->length);
    }

    std::span<const uint8_t> convertToSpan(const Token* token) const
    {
        return m_data.subspan(token->position, token->length);
    }

    [[nodiscard]] ParserStatus checkRequired()
    {
        auto sender = getString<49>(true);
        if (!sender || sender.value().size() == 0)
        {
            return ParserStatus::InvalidSenderCompId;
        }

        const auto target = getString<56>(true);
        if (!target || target.value().size() == 0)
        {
            return ParserStatus::InvalidTargetCompId;
        }

        const auto sequenceNumber = getUnsigned<34>(true);
        if (!sequenceNumber)
        {
            return ParserStatus::InvalidSequenceNumber;
        }
        return ParserStatus::Success;
    }
};
}

#endif //SIMD_FIX_MESSAGE_HPP
