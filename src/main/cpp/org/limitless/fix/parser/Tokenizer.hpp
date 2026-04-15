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

    const simd::Block FIELD_ENDS{0x01};
    const simd::Block TAG_ENDS{'='};
    const simd::Block ZEROS{'0'};
    const simd::Block NINES{'9'};
    const simd::Block TRUE{simd::Block::True};

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

    simd::Block m_data;

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

    void scan(const uint8_t* buffer, const size_t length)
    {
        uint8_t result[16];

        m_count = 0;
        for (size_t offset = 0; offset + 15 < length; offset += 16)
        {
            m_data.put(buffer + offset, length - offset);

            // A digit is valid if followed by '=' or a validated digit
            simd::Block digitFlags{m_data >= ZEROS & m_data <= NINES};
            simd::Block tagEnds{m_data == TAG_ENDS};
            simd::Block pb{digitFlags & tagEnds.shiftLeft<1>()};
            pb |= digitFlags & pb.shiftLeft<1>();
            pb |= digitFlags & pb.shiftLeft<1>();
            pb |= digitFlags & pb.shiftLeft<1>();
            // pb |= digitFlags & pb.shiftLeft<1>();

            // A digit is valid if preceded by 0x01 or a validated digit
            simd::Block fieldEnds{m_data == FIELD_ENDS};
            simd::Block pf = digitFlags & fieldEnds.shiftRight<1>();
            pf |= digitFlags & pf.shiftRight<1>();
            pf |= digitFlags & pf.shiftRight<1>();
            pf |= digitFlags & pf.shiftRight<1>();
            // pf |= digitFlags & pf.shiftRight<1>();

            simd::Block tagFlags{pb | pf};
            simd::Block tags{tagFlags.ifElse(m_data - ZEROS, TRUE)};

            //uint8x8_t narrowed = vshrn_n_u16(vreinterpretq_u16_u8(tagFlags.m_block), 4);
            //uint64_t packed = vget_lane_u64(vreinterpret_u64_u8(narrowed), 0);
            //std::printf("result = %016x\n", packed);
            //tags.get(0, result);
            //for (int i = 0; i < 16; ++i)
            //{
            //    std::printf("%03d ", result[i]);
            //}
            //std::printf("\n");
            tags.print();
            dump(16, buffer + offset);
            std::printf("\n");
        }
    }

private:

    void process(const uint8_t* block, const uint8_t* types, size_t remaining, Token& token)
    {
        // FIXME
    }

};
}

#endif //SIMD_FIX_TOKENIZER_H
