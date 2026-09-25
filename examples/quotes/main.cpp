//
// Encodes two Quote messages from quotes.xml into one buffer, then decodes the
// stream and prints each quote.
//
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string_view>

#include "org/limitless/simdifx/Fix.hpp"

using namespace org::limitless::simdifx;
using namespace org::limitless::simdifx::decoder;
using namespace org::limitless::simdifx::generated::messages;

namespace
{

struct QuoteHandler : FixMessageHandler<QuoteHandler>
{
    using FixMessageHandler::handle;

    Result handle(QuoteDecoder& quote)
    {
        const std::string_view id = quote.quoteID().value();
        const std::string_view symbol = quote.symbol().value();
        // QuoteType is optional in the spec: absent here means Null.
        const bool tradeable = quote.quoteType().value_or(QuoteType::Null) == QuoteType::Tradeable;
        std::printf("%-6.*s %-5.*s %5u @ %.2f / %.2f @ %-5u %s\n",
                    static_cast<int>(id.size()), id.data(),
                    static_cast<int>(symbol.size()), symbol.data(),
                    quote.bidSize().value(), quote.bidPx().value().toDouble(),
                    quote.offerPx().value().toDouble(), quote.offerSize().value(),
                    tradeable ? "tradeable" : "indicative");
        return Result::Success;
    }
};

}

int main()
{
    std::array<uint8_t, 1024> buffer{};
    FixPayloadEncoder encoder{Protocol::FIXT_1_1, "PRICER", "CLIENT"};
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());

    uint32_t length = 0;
    QuoteEncoder quote{};

    encoder.wrap(length, buffer);
    encoder.wrapMessage(quote)
        .sequenceNumber(1)
        .sendingTime(now)
        .quoteID("Q1")
        .symbol("AAPL")
        .quoteType(QuoteType::Tradeable)
        .bidPx(utils::FixedDecimal{15020, -2})
        .offerPx(utils::FixedDecimal{15025, -2})
        .bidSize(500)
        .offerSize(300)
        .transactTime(now);
    length += encoder.encode(quote);

    encoder.wrap(length, buffer);
    encoder.wrapMessage(quote)
        .sequenceNumber(2)
        .sendingTime(now)
        .quoteID("Q2")
        .symbol("MSFT")
        .bidPx(utils::FixedDecimal{41010, -2})
        .offerPx(utils::FixedDecimal{41030, -2})
        .bidSize(200)
        .offerSize(100);
    length += encoder.encode(quote);

    PayloadDecoder<Protocol::FIXT_1_1> decoder;
    QuoteHandler handler;
    std::span<const uint8_t> input{buffer.data(), length};
    while (!input.empty())
    {
        const auto [processed, status] = decoder.parse(input, handler);
        if (status == Result::MessageFragment || processed == 0)
        {
            break;
        }
        if (status != Result::Success)
        {
            std::printf("rejected: %.*s\n", static_cast<int>(name(status).size()), name(status).data());
        }
        input = input.subspan(processed);
    }
    return 0;
}
