//
// Created by Fredrik Dahlberg on 2026-04-20.
//
#define NDEBUG 1

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string_view>

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

// Logon message with a 3-entry HopsRepeatingGroup (tags 627/628/629).
static constexpr std::uint8_t LOGIN_GROUP[] =
    "8=FIXT.1.1" SOH
    "9=229" SOH
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
    "627=3" SOH
    "628=HOP1" SOH
    "629=20260609-12:13:14.000" SOH
    "628=HOP2" SOH
    "629=20260609-12:13:15.000" SOH
    "628=HOP3" SOH
    "629=20260609-12:13:16.000" SOH
    "10=151" SOH;

// Actual message length, excluding the C-string null terminator.
static constexpr size_t LOGIN_GROUP_LENGTH = sizeof(LOGIN_GROUP) - 1;

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
struct LogonGetterHandler : org::limitless::fix::messages::MessageHandler<LogonGetterHandler>
{
    using MessageHandler::handle;

    uint64_t sink = 0;

    org::limitless::fix::decoder::Result::Values handle(org::limitless::fix::messages::LogonDecoder& logon)
    {
        using org::limitless::fix::messages::Encryption;

        const auto sender = logon.sender().value_or(std::span<const uint8_t>{});
        const auto target = logon.target().value_or(std::span<const uint8_t>{});
        sink += sender.size();
        sink += target.size();
        sink += logon.sequenceNumber().value_or(0);
        sink += static_cast<uint64_t>(logon.sendingTime().value_or(std::chrono::milliseconds{0}).count());
        sink += logon.encryptMethod().value_or(Encryption{}).m_value;
        sink += logon.heartbeatInterval().value_or(0);
        return org::limitless::fix::decoder::Result::Success;
    }
};

// Applies every getter exposed by LogonDecoder, including iterating over the
// HopsRepeatingGroup, accumulating the results into a sink so the compiler
// cannot optimize the field accesses away.
struct LogonGroupGetterHandler : org::limitless::fix::messages::MessageHandler<LogonGroupGetterHandler>
{
    using MessageHandler::handle;

    uint64_t sink = 0;

    org::limitless::fix::decoder::Result::Values handle(org::limitless::fix::messages::LogonDecoder& logon)
    {
        using org::limitless::fix::messages::Encryption;

        const auto sender = logon.sender().value_or(std::span<const uint8_t>{});
        const auto target = logon.target().value_or(std::span<const uint8_t>{});
        sink += sender.size();
        sink += target.size();
        sink += logon.sequenceNumber().value_or(0);
        sink += static_cast<uint64_t>(logon.sendingTime().value_or(std::chrono::milliseconds{0}).count());
        sink += logon.encryptMethod().value_or(Encryption{}).m_value;
        sink += logon.heartbeatInterval().value_or(0);

        auto& hops = logon.hops();
        sink += hops.count();
        while (hops.hasNext())
        {
            hops.next();
            sink += hops.hopCompID().value_or(std::span<const uint8_t>{}).size();
            sink += static_cast<uint64_t>(hops.hopSendingTime().value_or(std::chrono::milliseconds{0}).count());
        }
        return org::limitless::fix::decoder::Result::Success;
    }
};

// COLD: 1 GB buffer — data comes from DRAM for most of the run.
static void benchColdCache()
{
    org::limitless::fix::decoder::PayloadDecoder decoder;

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
}

// HOT: 256 KB buffer — fits in L2, measures pure compute throughput.
static void benchHotCache()
{
    org::limitless::fix::decoder::PayloadDecoder decoder;

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
            (void) result;
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
                (void) result;
            }
        }
    });
    report("HOT CACHE", hotDuration, hotMsgs, LOGIN_LENGTH);
}

// GETTERS: parse + apply every LogonDecoder getter to the message.
static void benchGetters()
{
    org::limitless::fix::decoder::PayloadDecoder decoder;

    constexpr size_t HOT_SIZE  = 256 * 1024;
    constexpr size_t HOT_COUNT = 4096;
    auto hotBuf = std::make_unique<uint8_t[]>(HOT_SIZE);
    fillBuffer(hotBuf.get(), HOT_SIZE);

    constexpr size_t msgsPerPass = HOT_SIZE / LOGIN_LENGTH;
    constexpr size_t hotMsgs = msgsPerPass * HOT_COUNT;

    LogonGetterHandler handler{};
    const auto duration = timer([&]
    {
        for (size_t iter = 0; iter < HOT_COUNT; ++iter)
        {
            for (size_t i = 0; i < msgsPerPass; ++i)
            {
                const std::span<const uint8_t> bytes(&hotBuf[i * LOGIN_LENGTH], LOGIN_LENGTH);
                const auto result = decoder.parse(bytes, handler);
                (void) result;
            }
        }
    });
    std::printf("sink = %llu\n", static_cast<unsigned long long>(handler.sink));
    report("GETTERS", duration, hotMsgs, LOGIN_LENGTH);
}

// GROUPS: parse + apply every LogonDecoder getter, including a 3-entry hops
// repeating group, to the message.
static void benchGroups()
{
    org::limitless::fix::decoder::PayloadDecoder decoder;

    constexpr size_t HOT_SIZE  = 256 * 1024;
    constexpr size_t HOT_COUNT = 4096;
    auto hotBuf = std::make_unique<uint8_t[]>(HOT_SIZE);

    size_t i = 0;
    for (; i + LOGIN_GROUP_LENGTH <= HOT_SIZE; i += LOGIN_GROUP_LENGTH)
    {
        std::memcpy(&hotBuf[i], LOGIN_GROUP, LOGIN_GROUP_LENGTH);
    }
    std::memset(&hotBuf[i], ' ', HOT_SIZE - i);

    constexpr size_t msgsPerPass = HOT_SIZE / LOGIN_GROUP_LENGTH;
    constexpr size_t hotMsgs = msgsPerPass * HOT_COUNT;

    LogonGroupGetterHandler handler{};
    const auto duration = timer([&]
    {
        for (size_t iter = 0; iter < HOT_COUNT; ++iter)
        {
            for (size_t j = 0; j < msgsPerPass; ++j)
            {
                const std::span<const uint8_t> bytes(&hotBuf[j * LOGIN_GROUP_LENGTH], LOGIN_GROUP_LENGTH);
                const auto result = decoder.parse(bytes, handler);
                (void) result;
            }
        }
    });
    std::printf("sink = %llu\n", static_cast<unsigned long long>(handler.sink));
    report("GROUPS", duration, hotMsgs, LOGIN_GROUP_LENGTH);
}

struct Benchmark
{
    std::string_view name;
    void (*run)();
};

static constexpr Benchmark BENCHMARKS[] = {
    {"cold",    benchColdCache},
    {"hot",     benchHotCache},
    {"getters", benchGetters},
    {"groups",  benchGroups},
};

static void printUsage(const char* program)
{
    std::fprintf(stderr, "Usage: %s [all", program);
    for (const auto& benchmark : BENCHMARKS)
    {
        std::fprintf(stderr, "|%.*s", static_cast<int>(benchmark.name.size()), benchmark.name.data());
    }
    std::fprintf(stderr, "]...\n");
}

int main(const int argc, char** argv)
{
    std::printf("Message length = %zu bytes\n\n", LOGIN_LENGTH);

    if (argc < 2)
    {
        for (const auto& benchmark : BENCHMARKS)
        {
            benchmark.run();
        }
        return 0;
    }

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view name = argv[i];
        if (name == "all")
        {
            for (const auto& benchmark : BENCHMARKS)
            {
                benchmark.run();
            }
            continue;
        }

        const auto* found = std::ranges::find(BENCHMARKS, name, &Benchmark::name);
        if (found == std::end(BENCHMARKS))
        {
            std::fprintf(stderr, "Unknown benchmark '%s'\n", argv[i]);
            printUsage(argv[0]);
            return 1;
        }
        found->run();
    }
    return 0;
}