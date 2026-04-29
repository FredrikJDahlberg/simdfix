//
// Created by Fredrik Dahlberg on 2026-04-29.
//

#ifndef SIMD_FIX_HOP_GROUP_DECODER_HPP
#define SIMD_FIX_HOP_GROUP_DECODER_HPP

#include "org/limitless/fix/parser/MessageDecoder.hpp"
#include "org/limitless/fix/parser/GroupDecoder.hpp"

namespace org::limitless::fix::messages {

template <typename Message>
struct HopGroupDecoder : parser::GroupDecoder<Message>
{
    using Group = parser::GroupDecoder<Message>;

    static constexpr uint32_t Type = 10;

    explicit HopGroupDecoder(const Message* grammar) : Group(Type, grammar)
    {
    }

    HopGroupDecoder& wrap()
    {
        Group::prepare(Group::next(627));
        return *this;
    }

    HopGroupDecoder& clear()
    {
        Message::clear();
        return *this;
    }

    [[nodiscard]] std::expected<uint32_t, parser::ParserStatus> count() const
    {
        return Group::count();
    }

    HopGroupDecoder& next()
    {
        Group::next();
        return *this;
    }

    [[nodiscard]] std::expected<uint32_t, parser::ParserStatus> hopCompID() const
    {
        auto token = Group::next(628, Group::m_delim);
        if (token != nullptr)
        {
            return Group::m_message->convertToUnsigned(token);
        }
        auto required = Group::m_delim == 628;
        auto status = required ? parser::ParserStatus::RequiredFieldMissing : parser::ParserStatus::NullValue;
        return std::unexpected(status);
    }

    [[nodiscard]] std::expected<uint32_t, parser::ParserStatus> hopRefID() const
    {
        return Group::m_message->template getUnsigned<629>(Group::m_delim == 629);
    }
};
}

#endif //SIMD_FIX_HOP_GROUP_DECODER_HPP
