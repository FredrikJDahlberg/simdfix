#ifndef SIMD_FIX_MESSAGE_DECODERS_HPP
#define SIMD_FIX_MESSAGE_DECODERS_HPP

#include <expected>

#include "org/limitless/fix/decoder/DecoderStatus.hpp"
#include "org/limitless/fix/decoder/GroupDecoder.hpp"
#include "org/limitless/fix/decoder/MessageDecoder.hpp"
#include "org/limitless/fix/messages/Grammar.hpp"

namespace org::limitless::fix::generated {

using namespace org::limitless::fix;

template <typename Meta>
struct HopsDecoder : decoder::GroupDecoder<Meta>
{
public:
    HopsDecoder() = default;

    HopsDecoder(const Meta* message) :
        decoder::GroupDecoder<Meta>(message)
        {}

    // FIXME: this is null    HopsDecoder& wrap()
    {
        decoder::GroupDecoder<Meta>::wrap(decoder::GroupDecoder<Meta>::next(627));
        return *this;
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, decoder::DecoderStatus> hopCompID() const
    {
        return this->template getString<628>(false);
    }

    [[nodiscard]] std::expected<uint32_t, decoder::DecoderStatus> hopSendingTime() const
    {
        return this->template getUnsigned<629>(false);
    }

    [[nodiscard]] std::expected<uint32_t, decoder::DecoderStatus> hopRefID() const
    {
        return this->template getUnsigned<630>(false);
    }

};

template <typename Meta>
struct StandardHeaderDecoder : decoder::MessageDecoder<protocols::StandardHeader>
{
private:
    HopsDecoder<Meta> m_hops;

public:
    StandardHeaderDecoder() = default;

    StandardHeaderDecoder(const Meta* message) :
        m_hops{message}
        {}

    StandardHeaderDecoder& wrap(const std::span<const uint8_t> data,
                        const std::span<Token> tokens,
                        const std::span<uint16_t> tags,
                        const uint32_t count)
    {
        MessageDecoder::wrap(data, tokens, tags, count);
        m_hops.wrap();
        return *this;
    }

    HopsDecoder<Meta> hops()
    {
        return m_hops;
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, decoder::DecoderStatus> sender() const
    {
        return this->template getString<49>(true);
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, decoder::DecoderStatus> target() const
    {
        return this->template getString<56>(true);
    }

    [[nodiscard]] std::expected<uint32_t, decoder::DecoderStatus> sequenceNumber() const
    {
        return this->template getUnsigned<34>(true);
    }

    [[nodiscard]] std::expected<uint32_t, decoder::DecoderStatus> sendingTime() const
    {
        return this->template getUnsigned<52>(true);
    }

};

struct LogonDecoder : decoder::MessageDecoder<protocols::Logon>
{
    using Meta = decoder::MessageDecoder<protocols::Logon>;
private:
    StandardHeaderDecoder<Meta> m_standardHeader;

public:
    static constexpr uint16_t MessageId = 'A';

    LogonDecoder() = default;

    LogonDecoder(const Meta* message) :
        m_standardHeader{message}
        {}

    LogonDecoder& wrap(const std::span<const uint8_t> data,
                        const std::span<Token> tokens,
                        const std::span<uint16_t> tags,
                        const uint32_t count)
    {
        MessageDecoder::wrap(data, tokens, tags, count);
        m_standardHeader.wrap(data, tokens, tags, count);
        return *this;
    }

    StandardHeaderDecoder<Meta> standardHeader()
    {
        return m_standardHeader;
    }

    [[nodiscard]] std::expected<uint32_t, decoder::DecoderStatus> encryptMethod() const
    {
        return this->template getUnsigned<98>(false);
    }

    [[nodiscard]] std::expected<uint32_t, decoder::DecoderStatus> heartbeatInterval() const
    {
        return this->template getUnsigned<108>(true);
    }

};

struct LogoutDecoder : decoder::MessageDecoder<protocols::Logout>
{
    using Meta = decoder::MessageDecoder<protocols::Logout>;
private:
    StandardHeaderDecoder<Meta> m_standardHeader;

public:
    static constexpr uint16_t MessageId = '5';

    LogoutDecoder() = default;

    LogoutDecoder(const Meta* message) :
        m_standardHeader{message}
        {}

    LogoutDecoder& wrap(const std::span<const uint8_t> data,
                        const std::span<Token> tokens,
                        const std::span<uint16_t> tags,
                        const uint32_t count)
    {
        MessageDecoder::wrap(data, tokens, tags, count);
        m_standardHeader.wrap(data, tokens, tags, count);
        return *this;
    }

    StandardHeaderDecoder<Meta> standardHeader()
    {
        return m_standardHeader;
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, decoder::DecoderStatus> text() const
    {
        return this->template getString<58>(true);
    }

};

struct HeartbeatDecoder : decoder::MessageDecoder<protocols::Heartbeat>
{
    using Meta = decoder::MessageDecoder<protocols::Heartbeat>;
private:
    StandardHeaderDecoder<Meta> m_standardHeader;

public:
    static constexpr uint16_t MessageId = '0';

    HeartbeatDecoder() = default;

    HeartbeatDecoder(const Meta* message) :
        m_standardHeader{message}
        {}

    HeartbeatDecoder& wrap(const std::span<const uint8_t> data,
                        const std::span<Token> tokens,
                        const std::span<uint16_t> tags,
                        const uint32_t count)
    {
        MessageDecoder::wrap(data, tokens, tags, count);
        m_standardHeader.wrap(data, tokens, tags, count);
        return *this;
    }

    StandardHeaderDecoder<Meta> standardHeader()
    {
        return m_standardHeader;
    }

    [[nodiscard]] std::expected<std::span<const uint8_t>, decoder::DecoderStatus> testReqID() const
    {
        return this->template getString<112>(true);
    }

};

} // namespace org::limitless::fix::generated

#endif //SIMD_FIX_MESSAGE_DECODERS_HPP
