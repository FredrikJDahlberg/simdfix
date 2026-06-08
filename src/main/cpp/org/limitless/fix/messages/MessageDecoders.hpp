#ifndef SIMD_FIX_MESSAGE_DECODERS_HPP
#define SIMD_FIX_MESSAGE_DECODERS_HPP

#include <expected>

#include "org/limitless/fix/decoder/GroupDecoder.hpp"
#include "org/limitless/fix/decoder/StructDecoder.hpp"
#include "org/limitless/fix/decoder/MessageDecoder.hpp"
#include "org/limitless/fix/messages/Grammar.hpp"

namespace org::limitless::fix::generated {

using namespace org::limitless::fix::decoder;

using String = std::span<const uint8_t>;  // FIXME

struct Encryption
{
    enum Values
    {
        Null,
        None
    };
    static constexpr uint8_t Codes[2]  =
    {
        '?',
        '0'
    };
    Encryption() : m_value{Null} {}
    Values m_value;
};

struct HopsDecoder : GroupDecoder
{
    explicit HopsDecoder(FieldDecoder& decoder) : 
        GroupDecoder{decoder}
    {
    }

    HopsDecoder& wrap()
    {
        GroupDecoder::wrap(627);
        return *this;
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> hopCompID() const
    {
        return m_decoder.getString<628, false>();
    }

    [[nodiscard]] std::expected<std::int64_t, Result::Values> hopSendingTime() const
    {
        return m_decoder.getTimestamp<629, false>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> hopRefID() const
    {
        return m_decoder.getUint32<630, false>();
    }

};

struct StandardHeaderDecoder : StructDecoder
{
private:
    HopsDecoder m_hops;

public:
    explicit StandardHeaderDecoder(FieldDecoder& decoder) : 
        StructDecoder{decoder},
        m_hops{decoder}
    {
    }

    HopsDecoder& hops()
    {
        return m_hops.wrap();
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> sender() const
    {
        return m_decoder.getString<49, true>();
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> target() const
    {
        return m_decoder.getString<56, true>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> sequenceNumber() const
    {
        return m_decoder.getUint32<34, true>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> sendingTime() const
    {
        return m_decoder.getUint32<52, true>();
    }

};

struct LogonDecoder : MessageDecoder<protocols::Logon>
{
private:
    StandardHeaderDecoder m_standardHeader;

public:
    LogonDecoder() : 
        m_standardHeader{m_decoder}
    {
    }

    static constexpr uint16_t MessageId = 'A';

    LogonDecoder& wrap(const std::span<const uint8_t> data,
            const std::span<Token> tokens,
            const std::span<uint16_t> tags,
            const uint32_t count)
    {
        MessageDecoder::wrap(data, tokens, tags, count);
        return *this;
    }

    StandardHeaderDecoder& standardHeader()
    {
        return m_standardHeader;
    }

    [[nodiscard]] std::expected<Encryption, Result::Values> encryptMethod() const
    {
        return m_decoder.getEnum<98, false, Encryption>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> heartbeatInterval() const
    {
        return m_decoder.getUint32<108, true>();
    }

};

struct LogoutDecoder : MessageDecoder<protocols::Logout>
{
private:
    StandardHeaderDecoder m_standardHeader;

public:
    LogoutDecoder() : 
        m_standardHeader{m_decoder}
    {
    }

    static constexpr uint16_t MessageId = '5';

    LogoutDecoder& wrap(const std::span<const uint8_t> data,
            const std::span<Token> tokens,
            const std::span<uint16_t> tags,
            const uint32_t count)
    {
        MessageDecoder::wrap(data, tokens, tags, count);
        return *this;
    }

    StandardHeaderDecoder& standardHeader()
    {
        return m_standardHeader;
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> text() const
    {
        return m_decoder.getString<58, true>();
    }

};

struct HeartbeatDecoder : MessageDecoder<protocols::Heartbeat>
{
private:
    StandardHeaderDecoder m_standardHeader;

public:
    HeartbeatDecoder() : 
        m_standardHeader{m_decoder}
    {
    }

    static constexpr uint16_t MessageId = '0';

    HeartbeatDecoder& wrap(const std::span<const uint8_t> data,
            const std::span<Token> tokens,
            const std::span<uint16_t> tags,
            const uint32_t count)
    {
        MessageDecoder::wrap(data, tokens, tags, count);
        return *this;
    }

    StandardHeaderDecoder& standardHeader()
    {
        return m_standardHeader;
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> testReqID() const
    {
        return m_decoder.getString<112, false>();
    }

};

struct TestRequestDecoder : MessageDecoder<protocols::TestRequest>
{
private:
    StandardHeaderDecoder m_standardHeader;

public:
    TestRequestDecoder() : 
        m_standardHeader{m_decoder}
    {
    }

    static constexpr uint16_t MessageId = '1';

    TestRequestDecoder& wrap(const std::span<const uint8_t> data,
            const std::span<Token> tokens,
            const std::span<uint16_t> tags,
            const uint32_t count)
    {
        MessageDecoder::wrap(data, tokens, tags, count);
        return *this;
    }

    StandardHeaderDecoder& standardHeader()
    {
        return m_standardHeader;
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> testReqID() const
    {
        return m_decoder.getString<112, false>();
    }

};

} // namespace org::limitless::fix::generated

#endif //SIMD_FIX_MESSAGE_DECODERS_HPP
