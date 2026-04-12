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
private:
    static constexpr char ASSIGN = '=';
    static constexpr char SOH = '\x01';
    enum class State { TAG, VALUE };

    const uint8x16_t TagEnds;
    const uint8x16_t EndMask;
    const uint8x16_t Zeros;
    const uint8x16_t Nines;
    const uint8x16_t Invalid;

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

    Tokenizer() :
        TagEnds{vdupq_n_u8('=')},
        EndMask{vdupq_n_u8(0x01)},
        Zeros{vdupq_n_u8(0x30)},
        Nines{vdupq_n_u8(0x39)},
        Invalid{vdupq_n_u8(0x00)}
    {
    }

    // std::printf(" delim = %04x end = %04x digits = %04x\n",
    // move_mask_neon(delimChars), move_mask_neon(endChars), move_mask_neon(validDigits));

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

    //
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

/*
uint8x8_t narrowed = vshrn_n_u16(vreinterpretq_u16_u8(delims), 4);
uint64_t delimMask = vget_lane_u64(vreinterpret_u64_u8(narrowed), 0);
size_t count = 0;
for (int k = 0; k < 16; k++)
{
    if ((delimMask >> (k * 4)) & 0xF)
    {
        size_t indices[16];
        indices[count++] = k;
        std::cout << "pos = " << k << std::endl;
    }
}
*/
//const uint8x16_t equals = vceqq_u8(v_data, vdupq_n_u8('='));
//const uint8x16_t followers = vorrq_u8(digits, equals);
//const uint8_t nextByte = (i + 16 < length) ? data[i + 16] : 0x00;
//const bool nextIsFollower = (nextByte >= '0' && nextByte <= '9') || (nextByte == '=');
//const uint8x16_t followedByValid = vextq_u8(followers, vdupq_n_u8(nextIsFollower ? 0xFF : 0x00), 1);

//    enum Type { TAG, SEP, END, VALUE, COUNT, GROUP, LENGTH, DATA };

    // continue
//private:
/*
    bool process(const std::string_view buffer)
    {
        // std::cout << "buffer = " << buffer << std::endl;

        m_tokens[m_count] = { 0, 0, 0 };
        auto& token = m_tokens[m_count];

        size_t position = 0;
        size_t end = buffer.length();
        enum State { TAG, VALUE };
        State state = TAG;
//        int length = 0;
        int tag;
        while (position != end)
        {
            auto ch = buffer[position];
            // std::printf("%s %d %c\n", state == TAG ? "TAG" : "VALUE", position, ch);
            switch (state)
            {
                case TAG:
                    if (isdigit(ch))
                    {
                        token.tag *= 10;
                        token.tag += ch - '0';
                    }
                    else if (ch == ASSIGN)
                    {
                        token.valueOffset = position + 1;
                        state = VALUE;
                    }
                    else
                    {
                        return false;
                    }
                    break;
                case VALUE:
                    if (ch != SOH)
                    {
                        ++token.valueLength;
                    }
                    else
                    {
                        if (token.valueLength == 0)
                        {
                            return false;
                        }
                        std::printf("tag = %d, pos = %d, length = %d\n", token.tag, token.valueOffset, token.valueLength);
                        state = TAG;
                        token = m_tokens[++m_count];
                        token = { 0, 0, 0 };
                    }
            }
            ++position;
        }

        return false;
    }
*/
    /*
    constexpr std::int64_t indices(char digit, std::uint64_t value)
    {
        constexpr uint64_t ONES = 0x01010101'01010101ULL;
        constexpr uint64_t EIGHTS = 0x80808080'80808080ULL;
        const uint64_t mask = digit * ONES;
        const uint64_t x = value ^ mask;
        const uint64_t y = (x - ONES) & ~x & EIGHTS;
        return y;
    }
*/
    // Helper: Converts 128-bit NEON mask to a 16-bit integer (one bit per byte)
    static constexpr uint16_t move_mask_neon(uint8x16_t mask) {
        // 1. Shift bits so each 8-bit lane contains only 1 bit of interest
        // 2. Use a vertical add (pairwise) to collapse the bits into a single 64-bit value
        static const uint8x16_t bit_mask = {
            0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
            0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
        };

        // Mask the matching lanes so they only have one specific bit set
        // Sum up the bytes to pack them into two 64-bit halves
        const uint8x16_t masked = vandq_u8(mask, bit_mask);
        const uint64_t low = vaddv_u8(vget_low_u8(masked));
        const uint64_t high = vaddv_u8(vget_high_u8(masked));
        return static_cast<uint16_t>(low | (high << 8));
    }
};


}

#endif //SIMD_FIX_TOKENIZER_H


/*
 * https://www.onixs.biz/fix-dictionary/fixt1.1/compBlock_StandardHeader.html
Standard Message Header:
8	BeginString	Y  FIXT.1.1
9	BodyLength	Y
35	MsgType	Y
1128	ApplVerID	N
1156	ApplExtID	N
1129	CstmApplVerID	N
49	SenderCompID	Y
56	TargetCompID	Y
115	OnBehalfOfCompID	N
128	DeliverToCompID	    N

90	SecureDataLen	N
91	SecureData	N

34	MsgSeqNum	Y
50	SenderSubID	N
142	SenderLocationID	N
57	TargetSubID	N	'ADMIN' reserved for administrative messages
143	TargetLocationID	N
116	OnBehalfOfSubID	N
144	OnBehalfOfLocationID	N
129	DeliverToSubID	N
145	DeliverToLocationID	N
43	PossDupFlag	N
97	PossResend	N
52	SendingTime	Y
122	OrigSendingTime	N

212	XmlDataLen	N
213	XmlData	N

347	MessageEncoding	N
369	LastMsgSeqNumProcessed	N

627	NoHops	N
=>	628	HopCompID	N
=>	629	HopSendingTime	N
=>	630	HopRefID	N

Logon:
98	EncryptMethod	Y
108	HeartBtInt	Y

95	RawDataLength	N
96	RawData	N

141	ResetSeqNumFlag	N
789	NextExpectedMsgSeqNum	N

383	MaxMessageSize	N

84	NoMsgTypes	N
=>	372	RefMsgType	N
=>	385	MsgDirection	N
=>	1130	RefApplVerID	N
=>	1406	RefApplExtID	N
=>	1131	RefCstmApplVerID	N
=>	1410	DefaultVerIndicator	N

464	TestMessageIndicator	N
553	Username	N
554	Password	N
925	NewPassword	N
1400	EncryptedPasswordMethod	N

1401	EncryptedPasswordLen	N
1402	EncryptedPassword	N

1403	EncryptedNewPasswordLen	N
1404	EncryptedNewPassword	N

1409	SessionStatus	N
1137	DefaultApplVerID	Y
1407	DefaultApplExtID	N
1408	DefaultCstmApplVerID	N
58	Text	N

354	EncodedTextLen	N
355	EncodedText	N

Standard Message Trailer:
93	SignatureLength	N
89	Signature	N
10	CheckSum	Y
*/