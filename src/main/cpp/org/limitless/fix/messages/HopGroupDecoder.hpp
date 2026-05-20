//
// Created by Fredrik Dahlberg on 2026-04-29.
//

#ifndef SIMD_FIX_HOP_GROUP_DECODER_HPP
#define SIMD_FIX_HOP_GROUP_DECODER_HPP

#include "org/limitless/fix/decoder/MessageDecoder.hpp"
#include "org/limitless/fix/decoder/GroupDecoder.hpp"

namespace org::limitless::fix::messages {

template <typename Message>
struct HopGroupDecoder : decoder::GroupDecoder<Message>
{
    using Group = decoder::GroupDecoder<Message>;

    static constexpr uint32_t Type = 10;

    explicit HopGroupDecoder(const Message* grammar) : Group(Type, grammar)
    {
    }

    HopGroupDecoder& wrap()
    {
        Group::wrap(Group::next(627));
        return *this;
    }

    HopGroupDecoder& clear()
    {
        Group::clear();
        return *this;
    }

    [[nodiscard]] std::expected<uint32_t, decoder::DecoderStatus> count() const
    {
        return Group::count();
    }

    HopGroupDecoder& next()
    {
        Group::next();
        return *this;
    }

    [[nodiscard]] std::expected<uint32_t, decoder::DecoderStatus> hopCompID()
    {
        const auto token = Group::member(628);
        if (token != nullptr)
        {
            return Group::m_message->convertToUnsigned(token);
        }
        return std::unexpected{decoder::DecoderStatus::NullValue};
    }

    [[nodiscard]] std::expected<uint32_t, decoder::DecoderStatus> hopRefID()
    {
        const auto token = Group::member(629);
        if (token != nullptr)
        {
            return Group::m_message->convertToUnsigned(token);
        }
        return std::unexpected{decoder::DecoderStatus::NullValue};
    }
};
}

#endif //SIMD_FIX_HOP_GROUP_DECODER_HPP
