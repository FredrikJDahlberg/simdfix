#ifndef SIMD_FIX_MESSAGE_ENCODERS_HPP
#define SIMD_FIX_MESSAGE_ENCODERS_HPP

#include <expected>

// #include "org/limitless/fix/decoder/GroupDecoder.hpp"
// #include "org/limitless/fix/decoder/ComponentDecoder.hpp"
// #include "org/limitless/fix/decoder/MessageDecoder.hpp"

namespace org::limitless::fix::messages {

// using namespace org::limitless::fix::encoder;

    // Encryption
struct NestedGroupEncoder : GroupEncoder
{
    explicit NestedGroupEncoder(FieldEncoder& encoder) : 
        GroupEncoder{encoder}
    {
    }

};

struct HopsEncoder : GroupEncoder
{
private:
    NestedGroupEncoder m_nestedGroup;

public:
    explicit HopsEncoder(FieldEncoder& encoder) : 
        GroupEncoder{encoder},
        m_nestedGroup{encoder}
    {
    }

};

struct LogonEncoder : MessageEncoder
{
private:
    HopsEncoder m_hops;

public:
    LogonEncoder() : 
        m_hops{m_encoder}
    {
    }

    static constexpr uint16_t MessageId = 'A';

};

struct LogoutEncoder : MessageEncoder
{
private:
    HopsEncoder m_hops;

public:
    LogoutEncoder() : 
        m_hops{m_encoder}
    {
    }

    static constexpr uint16_t MessageId = '5';

};

struct HeartbeatEncoder : MessageEncoder
{
private:
    HopsEncoder m_hops;

public:
    HeartbeatEncoder() : 
        m_hops{m_encoder}
    {
    }

    static constexpr uint16_t MessageId = '0';

};

struct TestRequestEncoder : MessageEncoder
{
private:
    HopsEncoder m_hops;

public:
    TestRequestEncoder() : 
        m_hops{m_encoder}
    {
    }

    static constexpr uint16_t MessageId = '1';

};

} // namespace org::limitless::fix::messages

#endif //SIMD_FIX_MESSAGE_ENCODERS_HPP
