//
// Randomized buffer-safety fuzzing for PayloadDecoder.
//
// Every buffer is an exact-size heap allocation, so any read past
// [data, data + size) trips an AddressSanitizer redzone (the Debug build links
// ASan). Seeds are fixed so a failure is deterministically reproducible. The
// decoder must never read out of bounds and must never report Success for a
// truncated or malformed message.
//

#include <cstdint>
#include <cstring>
#include <random>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "org/limitless/simdifx/decoder/PayloadDecoder.hpp"

#include "org/limitless/simdifx/generated/messages/FixTypes.hpp"

namespace org::limitless::simdifx::decoder {

#define SOH "\x01"

namespace
{
    // Iterations per randomized case. Bumped low enough to keep ctest fast under
    // -O0 + ASan; raise locally for a deeper soak.
    constexpr int Iterations = 20'000;

    // tag 212 is the length tag for data tag 213, exercising emitDataSkip.
    struct DataFields
    {
        static constexpr int32_t dataTag(const uint16_t tag) { return tag == 212 ? 213 : -1; }
    };

    constexpr std::string_view ValidLogon =
        "8=FIXT.1.1" SOH "9=118" SOH "35=A" SOH "49=Buyer" SOH "56=SellerSide" SOH "34=1" SOH
        "52=20190605-11:51:27.84800" SOH "1128=9" SOH "98=0" SOH "108=30" SOH "141=Y" SOH
        "553=Username" SOH "554=Password" SOH "1137=9" SOH "10=218" SOH;

    constexpr std::string_view ValidData =
        "8=FIXT.1.1" SOH "9=0091" SOH "35=A" SOH "49=SENDER" SOH "56=TARGET" SOH "34=1" SOH
        "52=20260613-19:26:13.959" SOH "98=0" SOH "108=30" SOH "212=12" SOH "213=<root" SOH
        "/>test" SOH "10=127" SOH;

    // Parse a copy placed in an exact-size heap buffer so ASan guards the tail.
    ParseResult parseExact(std::span<const uint8_t> bytes)
    {
        std::vector<uint8_t> buffer(bytes.begin(), bytes.end());
        PayloadDecoder<generated::config::FIXT_1_1, DataFields> decoder;
        return decoder.parse(Buffer{buffer.data(), buffer.size()});
    }

    std::span<const uint8_t> bytesOf(const std::string_view text)
    {
        return {reinterpret_cast<const uint8_t*>(text.data()), text.size()};
    }
}

// Sanity: the valid messages parse from an exact-size buffer without an
// over-read (the trailing checksum SOH is the final byte, no padding behind it).
TEST(PayloadDecoderFuzz, ValidMessagesExactSize)
{
    EXPECT_EQ(Result::Success, parseExact(bytesOf(ValidLogon)).m_value);
    EXPECT_EQ(Result::Success, parseExact(bytesOf(ValidData)).m_value);
}

// Every truncation of each message: no over-read, never Success.
TEST(PayloadDecoderFuzz, AllTruncations)
{
    for (const auto full : {bytesOf(ValidLogon), bytesOf(ValidData)})
    {
        for (size_t n = 0; n < full.size(); ++n)
        {
            const auto status = parseExact(full.first(n)).m_value;
            ASSERT_NE(Result::Success, status) << "truncated to " << n << " bytes";
        }
    }
}

// Bit-flip mutation of a valid message body (BeginString prefix preserved), random truncation length.
TEST(PayloadDecoderFuzz, MutatedBody)
{
    const std::vector<uint8_t> base(ValidData.begin(), ValidData.end());
    std::mt19937 rng(0xC0FFEEu);
    std::uniform_int_distribution<int> byteDist(0, 255);
    std::uniform_int_distribution<size_t> lenDist(32, base.size());

    for (int iter = 0; iter < Iterations; ++iter)
    {
        std::vector<uint8_t> message = base;
        const size_t flips = 1 + (rng() % 24);
        for (size_t f = 0; f < flips; ++f)
        {
            message[11 + (rng() % (message.size() - 11))] = static_cast<uint8_t>(byteDist(rng));
        }
        // parseExact copies to an exact-size buffer; truncate via the span.
        parseExact(std::span{message}.first(lenDist(rng)));
    }
}

// Fully random body behind a valid "8=FIXT.1.1\x01" prefix.
TEST(PayloadDecoderFuzz, RandomBody)
{
    std::mt19937 rng(0xBADC0DEu);
    std::uniform_int_distribution<int> byteDist(0, 255);
    std::uniform_int_distribution<size_t> lenDist(32, 200);

    for (int iter = 0; iter < Iterations; ++iter)
    {
        const size_t n = lenDist(rng);
        std::vector<uint8_t> message(n);
        std::memcpy(message.data(), "8=FIXT.1.1\x01", 11);
        for (size_t i = 11; i < n; ++i)
        {
            message[i] = static_cast<uint8_t>(byteDist(rng));
        }
        const auto status = parseExact(message).m_value;
        // Random bytes effectively never form a valid checksummed message.
        ASSERT_NE(Result::Success, status) << "random length " << n;
    }
}

// Reuse one decoder across differently sized messages to surface stale state.
TEST(PayloadDecoderFuzz, DecoderReuse)
{
    PayloadDecoder<generated::config::FIXT_1_1, DataFields> decoder;
    const std::vector<uint8_t> logon(ValidLogon.begin(), ValidLogon.end());
    const std::vector<uint8_t> data(ValidData.begin(), ValidData.end());

    for (int i = 0; i < 200; ++i)
    {
        for (const auto& src : {data, logon})
        {
            for (size_t n = 32; n <= src.size(); n += 7)
            {
                std::vector<uint8_t> buffer(src.begin(), src.begin() + n);
                decoder.parse(Buffer{buffer.data(), buffer.size()});
            }
        }
    }

    std::vector buffer(logon);
    auto result = decoder.parse(Buffer{buffer.data(), buffer.size()});
    EXPECT_EQ(Result::Success, result.m_value) << name(result.m_value);
}

}
