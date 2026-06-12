#ifndef SIMD_FIX_MESSAGE_DECODERS_HPP
#define SIMD_FIX_MESSAGE_DECODERS_HPP

#include <expected>

#include "org/limitless/fix/decoder/GroupDecoder.hpp"
#include "org/limitless/fix/decoder/ComponentDecoder.hpp"
#include "org/limitless/fix/decoder/MessageDecoder.hpp"
#include "org/limitless/fix/messages/FixTypes.hpp"

namespace org::limitless::fix::messages {

using namespace org::limitless::fix::decoder;

struct NestedGroupDecoder : GroupDecoder
{
    explicit NestedGroupDecoder(FieldDecoder& decoder) : 
        GroupDecoder{decoder}
    {
    }

    NestedGroupDecoder& wrap()
    {
        GroupDecoder::wrap(500);
        return *this;
    }

    NestedGroupDecoder& next()
    {
        GroupDecoder::next();
        return *this;
    }

    [[nodiscard]] std::expected<std::string_view, Result::Values> name() const
    {
        return m_decoder.getString<501, false, ParentType::Group>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> nestedOne() const
    {
        return m_decoder.getUint32<601, false, ParentType::Group>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> nestedTwo() const
    {
        return m_decoder.getUint32<602, false, ParentType::Group>();
    }

};

struct HopsDecoder : GroupDecoder
{
private:
    NestedGroupDecoder m_nestedGroup;

public:
    explicit HopsDecoder(FieldDecoder& decoder) : 
        GroupDecoder{decoder},
        m_nestedGroup{decoder}
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

    [[nodiscard]] std::expected<std::string_view, Result::Values> hopCompID() const
    {
        return m_decoder.getString<628, false, ParentType::Group>();
    }

    [[nodiscard]] std::expected<std::chrono::milliseconds, Result::Values> hopSendingTime() const
    {
        return m_decoder.getTimestamp<629, false, ParentType::Group>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> hopRefID() const
    {
        return m_decoder.getUint32<630, false, ParentType::Group>();
    }

    NestedGroupDecoder& nestedGroup()
    {
        return m_nestedGroup.wrap();
    }

};

struct LogonDecoder : MessageDecoder
{
private:
    HopsDecoder m_hops;

public:
    LogonDecoder() : 
        m_hops{m_decoder}
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

    [[nodiscard]] std::expected<Protocol, Result::Values> beginString() const
    {
        return m_decoder.getEnum<8, false, Protocol, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> bodyLength() const
    {
        return m_decoder.getUint32<9, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<MessageType, Result::Values> msgType() const
    {
        return m_decoder.getEnum<35, false, MessageType, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::string_view, Result::Values> sender() const
    {
        return m_decoder.getString<49, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::string_view, Result::Values> target() const
    {
        return m_decoder.getString<56, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> sequenceNumber() const
    {
        return m_decoder.getUint32<34, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::chrono::milliseconds, Result::Values> sendingTime() const
    {
        return m_decoder.getTimestamp<52, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<Encryption, Result::Values> encryptMethod() const
    {
        return m_decoder.getEnum<98, false, Encryption, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> heartbeatInterval() const
    {
        return m_decoder.getUint32<108, false, ParentType::Message>();
    }

    HopsDecoder& hops()
    {
        return m_hops.wrap();
    }

};

struct LogoutDecoder : MessageDecoder
{
private:
    HopsDecoder m_hops;

public:
    LogoutDecoder() : 
        m_hops{m_decoder}
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

    [[nodiscard]] std::expected<Protocol, Result::Values> beginString() const
    {
        return m_decoder.getEnum<8, false, Protocol, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> bodyLength() const
    {
        return m_decoder.getUint32<9, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<MessageType, Result::Values> msgType() const
    {
        return m_decoder.getEnum<35, false, MessageType, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::string_view, Result::Values> sender() const
    {
        return m_decoder.getString<49, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::string_view, Result::Values> target() const
    {
        return m_decoder.getString<56, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> sequenceNumber() const
    {
        return m_decoder.getUint32<34, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::chrono::milliseconds, Result::Values> sendingTime() const
    {
        return m_decoder.getTimestamp<52, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::string_view, Result::Values> text() const
    {
        return m_decoder.getString<58, false, ParentType::Message>();
    }

    HopsDecoder& hops()
    {
        return m_hops.wrap();
    }

};

struct HeartbeatDecoder : MessageDecoder
{
private:
    HopsDecoder m_hops;

public:
    HeartbeatDecoder() : 
        m_hops{m_decoder}
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

    [[nodiscard]] std::expected<Protocol, Result::Values> beginString() const
    {
        return m_decoder.getEnum<8, false, Protocol, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> bodyLength() const
    {
        return m_decoder.getUint32<9, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<MessageType, Result::Values> msgType() const
    {
        return m_decoder.getEnum<35, false, MessageType, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::string_view, Result::Values> sender() const
    {
        return m_decoder.getString<49, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::string_view, Result::Values> target() const
    {
        return m_decoder.getString<56, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> sequenceNumber() const
    {
        return m_decoder.getUint32<34, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::chrono::milliseconds, Result::Values> sendingTime() const
    {
        return m_decoder.getTimestamp<52, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::string_view, Result::Values> testReqID() const
    {
        return m_decoder.getString<112, false, ParentType::Message>();
    }

    HopsDecoder& hops()
    {
        return m_hops.wrap();
    }

};

struct TestRequestDecoder : MessageDecoder
{
private:
    HopsDecoder m_hops;

public:
    TestRequestDecoder() : 
        m_hops{m_decoder}
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

    [[nodiscard]] std::expected<Protocol, Result::Values> beginString() const
    {
        return m_decoder.getEnum<8, false, Protocol, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> bodyLength() const
    {
        return m_decoder.getUint32<9, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<MessageType, Result::Values> msgType() const
    {
        return m_decoder.getEnum<35, false, MessageType, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::string_view, Result::Values> sender() const
    {
        return m_decoder.getString<49, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::string_view, Result::Values> target() const
    {
        return m_decoder.getString<56, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::uint32_t, Result::Values> sequenceNumber() const
    {
        return m_decoder.getUint32<34, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::chrono::milliseconds, Result::Values> sendingTime() const
    {
        return m_decoder.getTimestamp<52, false, ParentType::Message>();
    }

    [[nodiscard]] std::expected<std::string_view, Result::Values> testReqID() const
    {
        return m_decoder.getString<112, false, ParentType::Message>();
    }

    HopsDecoder& hops()
    {
        return m_hops.wrap();
    }

};

} // namespace org::limitless::fix::messages

#endif //SIMD_FIX_MESSAGE_DECODERS_HPP
