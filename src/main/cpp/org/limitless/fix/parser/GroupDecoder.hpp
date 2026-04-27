//
// Created by Fredrik Dahlberg on 2026-04-26.
//

#ifndef SIMD_FIX_GROUP_DECODER_HPP
#define SIMD_FIX_GROUP_DECODER_HPP

namespace org::limitless::fix::parser {

struct GroupDecoder
{
    using Token = Tokenizer::Token;
protected:
    MessageDecoder* m_message;
    Token* m_groupCount{};
    Token* m_group{};

    uint16_t m_delim{};

    uint32_t m_count{};
    int32_t m_position{};
    int32_t m_offset{};

public:
    explicit GroupDecoder(MessageDecoder* message) : m_message(message)
    {
    }

    GroupDecoder& wrap(MessageDecoder* message)
    {
        m_message = message;
        return *this;
    }
};

}

#endif //SIMD_FIX_GROUP_DECODER_HPP
