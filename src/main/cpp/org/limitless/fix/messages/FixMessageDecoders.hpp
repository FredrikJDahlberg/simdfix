#ifndef SIMD_FIX_MESSAGE_DECODERS_HPP
#define SIMD_FIX_MESSAGE_DECODERS_HPP

#include <expected>

#include "org/limitless/fix/decoder/GroupDecoder.hpp"
#include "org/limitless/fix/decoder/ComponentDecoder.hpp"
#include "org/limitless/fix/decoder/MessageDecoder.hpp"

namespace org::limitless::fix::generated {

using namespace org::limitless::fix::decoder;

using String = std::span<const uint8_t>;

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

struct TestDecoder : GroupDecoder
{
    explicit TestDecoder(FieldDecoder& decoder) : 
        GroupDecoder{decoder}
    {
    }

    TestDecoder& wrap()
    {
        GroupDecoder::wrap(500);
        return *this;
    }

    TestDecoder& next()
    {
        GroupDecoder::next();
        return *this;
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> name() const
    {
        return m_decoder.getString<501, false, ParentType::Group>();
    }

};

struct HopsDecoder : GroupDecoder
{
private:
    TestDecoder m_test;

public:
    explicit HopsDecoder(FieldDecoder& decoder) : 
        GroupDecoder{decoder},
        m_test{decoder}
    {
    }

    HopsDecoder& wrap()
    {
        GroupDecoder::wrap(627);
        return *this;
    }

    HopsDecoder& next()
    {
        GroupDecoder::next();
        return *this;
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> hopCompID() const
    {
        return m_decoder.getString<628, false, ParentType::Group>();
    }

    [[nodiscard]] std::expected<std::int64_t, Result::Values> hopSendingTime() const
    {
        return m_decoder.getTimestamp<629, false, ParentType::Group>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> hopRefID() const
    {
        return m_decoder.getUint32<630, false, ParentType::Group>();
    }

    TestDecoder& test()
    {
        return m_test.wrap();
    }

};

struct StandardHeaderDecoder : ComponentDecoder
{
private:
    HopsDecoder m_hops;

public:
    explicit StandardHeaderDecoder(FieldDecoder& decoder) : 
        ComponentDecoder{decoder},
        m_hops{decoder}
    {
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> sender() const
    {
        return m_decoder.getString<49, false, ParentType::Component>();
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> target() const
    {
        return m_decoder.getString<56, false, ParentType::Component>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> sequenceNumber() const
    {
        return m_decoder.getUint32<34, false, ParentType::Component>();
    }

    [[nodiscard]] std::expected<std::int64_t, Result::Values> sendingTime() const
    {
        return m_decoder.getTimestamp<52, false, ParentType::Component>();
    }

    HopsDecoder& hops()
    {
        return m_hops.wrap();
    }

};

struct LogonDecoder : MessageDecoder
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
        m_decoder.wrap(data, tokens, tags, count);
        return *this;
    }

    [[nodiscard]] std::expected<Encryption, Result::Values> encryptMethod() const
    {
        return m_decoder.getEnum<98, false, Encryption, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> heartbeatInterval() const
    {
        return m_decoder.getUint32<108, false, ParentType::Message>();
    }

    StandardHeaderDecoder& standardHeader()
    {
        return m_standardHeader;
    }

};

struct LogoutDecoder : MessageDecoder
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
        m_decoder.wrap(data, tokens, tags, count);
        return *this;
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> text() const
    {
        return m_decoder.getString<58, false, ParentType::Message>();
    }

    StandardHeaderDecoder& standardHeader()
    {
        return m_standardHeader;
    }

};

struct HeartbeatDecoder : MessageDecoder
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
        m_decoder.wrap(data, tokens, tags, count);
        return *this;
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> testReqID() const
    {
        return m_decoder.getString<112, false, ParentType::Message>();
    }

    StandardHeaderDecoder& standardHeader()
    {
        return m_standardHeader;
    }

};

struct TestRequestDecoder : MessageDecoder
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
        m_decoder.wrap(data, tokens, tags, count);
        return *this;
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, Result::Values> testReqID() const
    {
        return m_decoder.getString<112, false, ParentType::Message>();
    }

    StandardHeaderDecoder& standardHeader()
    {
        return m_standardHeader;
    }

};

} // namespace org::limitless::fix::generated

#endif //SIMD_FIX_MESSAGE_DECODERS_HPP
