//
// Created by Fredrik Dahlberg on 2026-04-20.
//
#define NDEBUG 1

#include <memory>
#include <cstddef>
#include <chrono>

#include "org/limitless/fix/decoder/PayloadDecoder.hpp"
#include "org/limitless/fix/messages/FixMessageDecoders.hpp"
#include "org/limitless/fix/messages/FixMessageHandler.hpp"

#define SOH "\x01"

static constexpr std::uint8_t LOGIN[] =
    "8=FIXT.1.1" SOH
    "9=118" SOH
    "35=A" SOH
    "49=Buyer" SOH
    "56=SellSide_1" SOH
    "34=1" SOH
    "52=20190605-11:51:27.84800" SOH
    "1128=9" SOH
    "98=0" SOH
    "108=30" SOH
    "141=Y" SOH
    "553=Username" SOH
    "554=Password" SOH
    "1137=9" SOH
    "10=147" SOH;

// Actual message length, excluding the C-string null terminator.
static constexpr size_t LOGIN_LENGTH = sizeof(LOGIN) - 1;

using namespace std::chrono;

template <typename Handler>
nanoseconds timer(Handler handler)
{
    const auto start = high_resolution_clock::now();
    handler();
    return nanoseconds(high_resolution_clock::now() - start);
}

static void fillBuffer(uint8_t* buf, const size_t bufSize)
{
    size_t i = 0;
    for (; i + LOGIN_LENGTH <= bufSize; i += LOGIN_LENGTH)
    {
        std::memcpy(&buf[i], LOGIN, LOGIN_LENGTH);
    }
    std::memset(&buf[i], ' ', bufSize - i);
}

static void report(const char* label, const nanoseconds duration, const size_t msgCount, const size_t msgBytes)
{
    const auto ms       = duration_cast<milliseconds>(duration).count();
    const auto seconds  = ::duration<double>(duration).count();
    const auto gbps     = static_cast<double>(msgCount) * static_cast<double>(msgBytes) / 1e9 / seconds;
    const auto nsPerMsg = static_cast<double>(duration.count()) / static_cast<double>(msgCount);
    std::printf("%-12s %6lld ms   %6.3f GB/s   %5.1f ns/msg\n", label, ms, gbps, nsPerMsg);
}

// Applies every getter exposed by LogonDecoder, accumulating the results into
// a sink so the compiler cannot optimize the field accesses away.
struct LogonGetterHandler : org::limitless::fix::generated::MessageHandler<LogonGetterHandler>
{
    using MessageHandler::handle;

    uint64_t sink = 0;

    org::limitless::fix::decoder::Result::Values handle(org::limitless::fix::generated::LogonDecoder& logon)
    {
        using org::limitless::fix::generated::Encryption;

        const auto sender = logon.sender().value_or(std::span<const uint8_t>{});
        const auto target = logon.target().value_or(std::span<const uint8_t>{});
        sink += sender.size();
        sink += target.size();
        sink += logon.sequenceNumber().value_or(0);
        sink += static_cast<uint64_t>(logon.sendingTime().value_or(0));
        sink += logon.encryptMethod().value_or(Encryption{}).m_value;
        sink += logon.heartbeatInterval().value_or(0);
        return org::limitless::fix::decoder::Result::Success;
    }
};

static nanoseconds runGetterBenchmark(org::limitless::fix::decoder::PayloadDecoder& decoder,
                                       uint8_t* buf, const size_t msgsPerPass, const size_t iterations)
{
    LogonGetterHandler handler{};
    const auto duration = timer([&]
    {
        for (size_t iter = 0; iter < iterations; ++iter)
        {
            for (size_t i = 0; i < msgsPerPass; ++i)
            {
                const std::span<const uint8_t> bytes(&buf[i * LOGIN_LENGTH], LOGIN_LENGTH);
                const auto result = decoder.parse(bytes, handler);
                (void) result;
            }
        }
    });
    std::printf("sink = %llu\n", static_cast<unsigned long long>(handler.sink));
    return duration;
}

int main()
{
    org::limitless::fix::decoder::PayloadDecoder decoder;

    std::printf("Message length = %zu bytes\n\n", LOGIN_LENGTH);

    // COLD: 1 GB buffer — data comes from DRAM for most of the run.
    constexpr size_t COLD_SIZE = 1024ULL * 1024 * 1024;
    auto coldBuf = std::make_unique<uint8_t[]>(COLD_SIZE);
    fillBuffer(coldBuf.get(), COLD_SIZE);

    constexpr size_t coldMessages = COLD_SIZE / LOGIN_LENGTH;
    const auto coldDuration = timer([&]
    {
        for (size_t i = 0; i < coldMessages; ++i)
        {
            const std::span<const uint8_t> bytes(&coldBuf[i * LOGIN_LENGTH], LOGIN_LENGTH);
            (void) decoder.parse(bytes);
        }
    });
    report("COLD CACHE", coldDuration, coldMessages, LOGIN_LENGTH);

    // HOT: 256 KB buffer — fits in L2, measures pure compute throughput.
    constexpr size_t HOT_SIZE  = 256 * 1024;
    constexpr size_t HOT_COUNT = 4096;
    auto hotBuf = std::make_unique<uint8_t[]>(HOT_SIZE);
    fillBuffer(hotBuf.get(), HOT_SIZE);

    // Warm up i-cache and d-cache before timing.
    constexpr size_t msgsPerPass = HOT_SIZE / LOGIN_LENGTH;
    for (int w = 0; w < 4; ++w)
    {
        for (size_t i = 0; i < msgsPerPass; ++i)
        {
            const std::span<const uint8_t> bytes(&hotBuf[i * LOGIN_LENGTH], LOGIN_LENGTH);
            const auto result = decoder.parse(bytes);
            (void)result;
        }
    }

    constexpr size_t hotMsgs = msgsPerPass * HOT_COUNT;
    const auto hotDuration = timer([&]
    {
        for (size_t iter = 0; iter < HOT_COUNT; ++iter)
        {
            for (size_t i = 0; i < msgsPerPass; ++i)
            {
                const std::span<const uint8_t> bytes(&hotBuf[i * LOGIN_LENGTH], LOGIN_LENGTH);
                const auto result = decoder.parse(bytes);
                (void)result;
            }
        }
    });
    report("HOT CACHE", hotDuration, hotMsgs, LOGIN_LENGTH);

    // GETTERS: parse + apply every LogonDecoder getter to the message.
    const auto getterDuration = runGetterBenchmark(decoder, hotBuf.get(), msgsPerPass, HOT_COUNT);
    report("GETTERS", getterDuration, hotMsgs, LOGIN_LENGTH);

    return 0;
}
