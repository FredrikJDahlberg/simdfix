//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#ifndef SIMD_FIX_TOKENIZER_H
#define SIMD_FIX_TOKENIZER_H

#include <ostream>

#include "org/limitless/fix/parser/Block.hpp"

namespace org::limitless::fix::parser {

class Tokenizer
{
    static constexpr char TAG_END = '=';
    static constexpr char FIELD_END = '\x01';

    static const uint8x16_t TagEnds;
    static const uint8x16_t EndMask;
    static const uint8x16_t Zeros;
    static const uint8x16_t Nines;
    static const uint8x16_t False;
    static const uint8x16_t True;

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

    Tokenizer() = default;

    static void dump(size_t length, const uint8_t* buffer)
    {
        for (int i = 0; i < length; ++i)
        {
            if (const auto ch = buffer[i]; std::isprint(ch))
            {
                std::printf("%2c ", ch);
            }
            else
            {
                std::printf("%2c ", ch == 1 ? '|' : '?');
            }
        }
    }

    static void dump(const uint8x16_t& vector)
    {
        const auto data = reinterpret_cast<const uint8_t*>(&vector);
        for (int j = 0; j < 16; ++j)
        {
            std::printf("%02x ", data[j]);
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

    static void scan(const uint8_t* buffer, const size_t length)
    {
        size_t i = 0;
        simd::Block data;

        for (; i + 15 < length; i += 16)
        {
            const uint8x16_t data = vld1q_u8(reinterpret_cast<const uint8_t*>(buffer + i));
            const uint8x16_t digitFlags = vandq_u8(vcgeq_u8(data, vdupq_n_u8('0')), vcleq_u8(data, vdupq_n_u8('9')));

            // 2. Unrolled Backward Propagation (Right to Left)
            // A digit is valid if followed by '=' or a validated digit
            const uint8x16_t tagEnds = vceqq_u8(data, vdupq_n_u8(TAG_END));
            uint8x16_t pb = vandq_u8(digitFlags, vextq_u8(tagEnds, False, 1));
            pb = vorrq_u8(pb, vandq_u8(digitFlags, vextq_u8(pb, False, 1)));
            pb = vorrq_u8(pb, vandq_u8(digitFlags, vextq_u8(pb, False, 1)));
            pb = vorrq_u8(pb, vandq_u8(digitFlags, vextq_u8(pb, False, 1)));
            // pb = vorrq_u8(pb, vandq_u8(digitFlags, vextq_u8(pb, Zeros, 1)));

            // 3. Unrolled Forward Propagation (Left to Right)
            // A digit is valid if preceded by 0x01 or a validated digit
            const uint8x16_t fieldEnds   = vceqq_u8(data, vdupq_n_u8(FIELD_END));
            uint8x16_t pf = vandq_u8(digitFlags, vextq_u8(False, fieldEnds, 15));
            pf = vorrq_u8(pf, vandq_u8(digitFlags, vextq_u8(False, pf, 15)));
            pf = vorrq_u8(pf, vandq_u8(digitFlags, vextq_u8(False, pf, 15)));
            pf = vorrq_u8(pf, vandq_u8(digitFlags, vextq_u8(False, pf, 15)));
            //pf = vorrq_u8(pf, vandq_u8(digitFlags, vextq_u8(Zeros, pf, 15)));

            const uint8x16_t tagFlags = vorrq_u8(pb, pf);
            uint8x16_t tags = vbslq_u8(tagFlags, vsubq_u8(data, Zeros), True);
            dump(tagFlags);
            dump(tags);
            dump(16, buffer + i);
            std::printf("\n");
        }
    }

    void scanBlock(const uint8_t* buffer, const size_t length)
    {
        size_t offset = 0;
        m_count = 0;

        const simd::Block fieldEnds{0x01};
        const simd::Block tagEnds{'='};
        const simd::Block zeros{'0'};
        const simd::Block nines{'9'};
        simd::Block block;

        const auto start = std::chrono::high_resolution_clock::now();

        //for (int i = 0; i < 1'000'000; ++i)
        {
            uint8_t r[16];
            for (; offset + 15 < length; offset += 16)
            {
                block.put(buffer + offset, length - offset);
                simd::Block delimFlags = block == tagEnds | block == fieldEnds;
                delimFlags.shiftLeft<1>();
                delimFlags.print();

                simd::Block digitFlags = block >= zeros & block <= nines;
                digitFlags.print();
                simd::Block digits = block - zeros;
                digits.print();
                // simd::Block result = digitFlags.ifElse(block - zeros, delimFlags);
                // result.print();

                // calc absolute pos bitmaps for tag and field ends
                // token = tag pos + tag length + value length
                for (size_t j = offset; j < offset + 16; ++j)
                {
                    std::printf("%2c ", buffer[j]);
                }
                std::printf("\n");


            }
        }
        const auto end = std::chrono::high_resolution_clock::now();
        const auto  duration = std::chrono::nanoseconds(end - start);
        std::printf("%8lld\n", duration.count());

        //
        for (; offset < length; ++offset)
        {
         //   std::printf("todo = %c\n", buffer[i]);
        }
    }

};
}

#endif //SIMD_FIX_TOKENIZER_H
