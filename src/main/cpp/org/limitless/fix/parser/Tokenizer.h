//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#ifndef SIMD_FIX_TOKENIZER_H
#define SIMD_FIX_TOKENIZER_H

#include <ostream>
#include <arm_neon.h>

namespace org::limitless::fix::parser {

class Tokenizer
{
    static constexpr char TAG_END = '=';
    static constexpr char FIELD_END = '\x01';

    static const uint8x16_t TagEnds;
    static const uint8x16_t EndMask;
    static const uint8x16_t Zeros;
    static const uint8x16_t Nines;
    static const uint8x16_t Invalid;

    enum class State { TAG, VALUE };
    struct Token
    {
        int tag;
        int valueOffset;
        int valueLength;
    };

    Token m_tokens[128]{};
    size_t m_count = 0;
    State m_state = State::TAG;

public:
    // produce a list of { | tag, offset, length, type | should be sorted by tag or group
    // order, body = tag, group = pos, data = len + data

    Tokenizer()
    {
    }

    static void dump(const uint8x16_t& vector)
    {
        const auto data = reinterpret_cast<const uint8_t*>(&vector);
        for (int j = 0; j < 16; ++j)
        {
            std::printf("%3d ", data[j]);
        }
        std::printf("\n");
    }

    /*
    void process(const uint8_t* block, const uint8_t* types, size_t remaining, Token& token)
    {
        for (size_t i = 0; i < remaining; ++i)
        {
            const uint8_t ch = block[i];

            switch (m_state)
            {
                case State::TAG:
                    token.tag *= 10;
                    token.tag += ch;
                    break;
                case State::VALUE:
                    token.valueOffset += ch;

            }
        }
    }
    */

    void scan(const uint8_t* buffer, const size_t length)
    {
        size_t i = 0;
        m_count = 0;
        for (; i + 15 < length; i += 16)
        {
            const uint8x16_t data = vld1q_u8(reinterpret_cast<const uint8_t*>(buffer + i));
            const uint8x16_t tagEnds = vceqq_u8(data, TagEnds);
            const uint8x16_t fieldEnds = vceqq_u8(data, EndMask);
            const uint8x16_t ends = vorrq_u8(tagEnds, fieldEnds);
            const uint8x16_t digits = vandq_u8(vcgeq_u8(data, Zeros), vcleq_u8(data, Nines));
            const uint8x16_t tags = vsubq_u8(data, Zeros);
            const uint8x16_t validDigits = vbslq_u8(digits, tags, ends);

            for (size_t j = i; j < i + 16; ++j)
            {
                std::printf("%3c ", data[j]);
            }
            std::printf("\n");
            dump(validDigits);
            std::printf("\n");
        }

        //
        for (; i < length; ++i)
        {
            std::printf("todo = %c\n", buffer[i]);
        }
    }
};
}

#endif //SIMD_FIX_TOKENIZER_H
