#ifndef SIMD_FIX_MESSAGE_DECODERS_HPP
#define SIMD_FIX_MESSAGE_DECODERS_HPP

#include <expected>

#include "org/limitless/fix/decoder/GroupDecoder.hpp"
#include "org/limitless/fix/decoder/StructDecoder.hpp"
#include "org/limitless/fix/decoder/MessageDecoder.hpp"
#include "org/limitless/fix/messages/Grammar.hpp"

namespace org::limitless::fix::generated {

using namespace org::limitless::fix::decoder;

struct HopsDecoder : GroupDecoder
{
    HopsDecoder() = default;

    HopsDecoder& wrap(FieldDecoder* decoder, uint32_t tag)
    {
        GroupDecoder::wrap(decoder, tag);
        return *this;
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> hopCompID() const
    {
        return m_decoder->getString<628, false>();
    }

    [[nodiscard]] std::expected<uint32_t, Result::Values> hopSendingTime() const
    {
        return m_decoder->getUint32<629, false>();
    }

    [[nodiscard]] std::expected<uint32_t, Result::Values> hopRefID() const
    {
        return m_decoder->getUint32<630, false>();
    }

};

struct StandardHeaderDecoder : StructDecoder
{
    StandardHeaderDecoder() = default;

private:
    HopsDecoder m_hops{};

public:
    StandardHeaderDecoder& wrap(FieldDecoder* decoder)
    {
        StructDecoder::wrap(decoder);
        m_hops.wrap(decoder, 627);
        return *this;
    }

    HopsDecoder& hops()
    {
        return m_hops;
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> sender() const
    {
        return m_decoder->getString<49, true>();
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> target() const
    {
        return m_decoder->getString<56, true>();
    }

    [[nodiscard]] std::expected<uint32_t, Result::Values> sequenceNumber() const
    {
        return m_decoder->getUint32<34, true>();
    }

    [[nodiscard]] std::expected<uint32_t, Result::Values> sendingTime() const
    {
        return m_decoder->getUint32<52, true>();
    }

};

struct LogonDecoder : MessageDecoder<protocols::Logon>
{
    using Decoder = MessageDecoder;

    LogonDecoder() = default;

private:
    StandardHeaderDecoder m_standardHeader{};

public:
    static constexpr uint16_t MessageId = 'A';

    LogonDecoder& wrap(const std::span<const uint8_t> data,
                        const std::span<Token> tokens,
                        const std::span<uint16_t> tags,
                        const uint32_t count)
    {
        Decoder::wrap(data, tokens, tags, count);
        m_standardHeader.wrap(m_decoder);
        return *this;
    }

    StandardHeaderDecoder& standardHeader()
    {
        return m_standardHeader;
    }

    [[nodiscard]] std::expected<uint32_t, Result::Values> encryptMethod() const
    {
        return m_decoder->getUint32<98, false>();
    }

    [[nodiscard]] std::expected<uint32_t, Result::Values> heartbeatInterval() const
    {
        return m_decoder->getUint32<108, true>();
    }

};

struct LogoutDecoder : MessageDecoder<protocols::Logout>
{
    using Decoder = MessageDecoder;

    LogoutDecoder() = default;

private:
    StandardHeaderDecoder m_standardHeader{};

public:
    static constexpr uint16_t MessageId = '5';

    LogoutDecoder& wrap(const std::span<const uint8_t> data,
                        const std::span<Token> tokens,
                        const std::span<uint16_t> tags,
                        const uint32_t count)
    {
        Decoder::wrap(data, tokens, tags, count);
        m_standardHeader.wrap(m_decoder);
        return *this;
    }

    StandardHeaderDecoder& standardHeader()
    {
        return m_standardHeader;
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> text() const
    {
        return m_decoder->getString<58, true>();
    }

};

struct HeartbeatDecoder : MessageDecoder<protocols::Heartbeat>
{
    using Decoder = MessageDecoder;

    HeartbeatDecoder() = default;

private:
    StandardHeaderDecoder m_standardHeader{};

public:
    static constexpr uint16_t MessageId = '0';

    HeartbeatDecoder& wrap(const std::span<const uint8_t> data,
                        const std::span<Token> tokens,
                        const std::span<uint16_t> tags,
                        const uint32_t count)
    {
        Decoder::wrap(data, tokens, tags, count);
        m_standardHeader.wrap(m_decoder);
        return *this;
    }

    StandardHeaderDecoder& standardHeader()
    {
        return m_standardHeader;
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> testReqID() const
    {
        return m_decoder->getString<112, true>();
    }

};

} // namespace org::limitless::fix::generated

#endif //SIMD_FIX_MESSAGE_DECODERS_HPP
