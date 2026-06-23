//
// Created by Fredrik Dahlberg on 2026-04-20.
//
#define NDEBUG 1

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string_view>

#include "org/limitless/fix/decoder/PayloadDecoder.hpp"
#include "org/limitless/fix/messages/FixMessageDecoders.hpp"
#include "org/limitless/fix/messages/FixMessageEncoders.hpp"
#include "org/limitless/fix/messages/FixMessageHandler.hpp"

#define SOH "\x01"

using namespace org::limitless::fix;

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

// Logon message with a 12-byte XmlData payload containing an embedded SOH.
static constexpr std::uint8_t LOGIN_DATA[] =
    "8=FIXT.1.1" SOH
    "9=142" SOH
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
    "212=12" SOH
    "213=<root" SOH "/>test" SOH
    "10=200" SOH;

static constexpr size_t LOGIN_DATA_LENGTH = sizeof(LOGIN_DATA) - 1;

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

static constexpr std::uint8_t NEW_ORDER_SINGLE[] =
    "8=FIXT.1.1" SOH
    "9=0129" SOH
    "35=D" SOH
    "49=SENDER" SOH
    "56=TARGET" SOH
    "34=1" SOH
    "52=20260613-19:26:13.959" SOH
    "11=ORDER1" SOH
    "21=1" SOH
    "55=AAPL" SOH
    "54=1" SOH
    "60=20260613-19:26:13.959" SOH
    "38=100" SOH
    "40=2" SOH
    "44=15000" SOH
    "10=126" SOH;

static constexpr size_t NEW_ORDER_SINGLE_LENGTH = sizeof(NEW_ORDER_SINGLE) - 1;

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
    std::printf("%-12s %6lld ms   %6.3f GB/s   %5.1f ns/msg\n", label, static_cast<long long>(ms), gbps, nsPerMsg);
}

// Applies every getter exposed by LogonDecoder, accumulating the results into
// a sink so the compiler cannot optimize the field accesses away.
struct LogonGetterHandler : org::limitless::fix::messages::MessageHandler<LogonGetterHandler>
{
    using MessageHandler::handle;

    uint64_t sink = 0;

    org::limitless::fix::Result::Values handle(org::limitless::fix::messages::LogonDecoder& logon)
    {
        using org::limitless::fix::messages::Encryption;

        const auto sender = logon.sender().value_or(std::string_view{});
        const auto target = logon.target().value_or(std::string_view{});
        sink += sender.size();
        sink += target.size();
        sink += logon.sequenceNumber().value_or(0);
        sink += static_cast<uint64_t>(logon.sendingTime().value_or(std::chrono::milliseconds{0}).count());
        sink += logon.encryptMethod().value_or(Encryption::None);
        sink += logon.heartbeatInterval().value_or(0);
        return org::limitless::fix::Result::Success;
    }
};

// Applies every getter exposed by LogonDecoder, including iterating over the
// HopsRepeatingGroup, accumulating the results into a sink so the compiler
// cannot optimize the field accesses away.
struct LogonGroupGetterHandler : org::limitless::fix::messages::MessageHandler<LogonGroupGetterHandler>
{
    using MessageHandler::handle;

    uint64_t sink = 0;

    org::limitless::fix::Result::Values handle(org::limitless::fix::messages::LogonDecoder& logon)
    {
        using org::limitless::fix::messages::Encryption;

        const auto sender = logon.sender().value_or(std::string_view{});
        const auto target = logon.target().value_or(std::string_view{});
        sink += sender.size();
        sink += target.size();
        sink += logon.sequenceNumber().value_or(0);
        sink += static_cast<uint64_t>(logon.sendingTime().value_or(std::chrono::milliseconds{0}).count());
        sink += logon.encryptMethod().value_or(Encryption::None);
        sink += logon.heartbeatInterval().value_or(0);

        auto& hops = logon.hops();
        sink += hops.count();
        while (hops.hasNext())
        {
            hops.next();
            sink += hops.hopCompID().value_or(std::string_view{}).size();
            sink += static_cast<uint64_t>(hops.hopSendingTime().value_or(std::chrono::milliseconds{0}).count());
        }
        return org::limitless::fix::Result::Success;
    }
};

struct LogonDataGetterHandler : org::limitless::fix::messages::MessageHandler<LogonDataGetterHandler>
{
    using MessageHandler::handle;

    uint64_t sink = 0;

    org::limitless::fix::Result::Values handle(org::limitless::fix::messages::LogonDecoder& logon)
    {
        using org::limitless::fix::messages::Encryption;

        sink += logon.sender().value_or(std::string_view{}).size();
        sink += logon.target().value_or(std::string_view{}).size();
        sink += logon.sequenceNumber().value_or(0);
        sink += static_cast<uint64_t>(logon.sendingTime().value_or(std::chrono::milliseconds{0}).count());
        sink += logon.encryptMethod().value_or(Encryption::None);
        sink += logon.heartbeatInterval().value_or(0);
        const auto data = logon.xmlData().get();
        if (data.has_value())
        {
            sink += data.value().size();
        }
        return org::limitless::fix::Result::Success;
    }
};

// COLD: 1 GB buffer — data comes from DRAM for most of the run.
static void benchColdCache()
{
    decoder::PayloadDecoder<FIXT_1_1> decoder;

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
    report("LOGON COLD", coldDuration, coldMessages, LOGIN_LENGTH);
}

// HOT: 256 KB buffer — fits in L2, measures pure compute throughput.
static void benchHotCache()
{
    decoder::PayloadDecoder<FIXT_1_1> decoder;

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
    report("LOGON HOT", hotDuration, hotMsgs, LOGIN_LENGTH);
}

// GETTERS: parse + apply every LogonDecoder getter to the message.
static void benchGetters()
{
    decoder::PayloadDecoder<FIXT_1_1> decoder;

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
    report("LOGON GET", duration, hotMsgs, LOGIN_LENGTH);
}

// GROUPS: parse + apply every LogonDecoder getter, including a 3-entry hops
// repeating group, to the message.
static void benchGroups()
{
    decoder::PayloadDecoder<FIXT_1_1> decoder;

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
    report("LOGON GRP", duration, hotMsgs, LOGIN_GROUP_LENGTH);
}

// LOGON_DATA: hot-cache decode + getters of Logon with XmlData (inline skip path).
static void benchLogonData()
{
    struct DataFields
    {
        static constexpr int32_t dataTag(const uint16_t tag)
        {
            if (tag == 212) return 213;
            return -1;
        }
    };
    decoder::PayloadDecoder<FIXT_1_1, DataFields> decoder;

    constexpr size_t HOT_SIZE  = 256 * 1024;
    constexpr size_t HOT_COUNT = 4096;
    auto hotBuf = std::make_unique<uint8_t[]>(HOT_SIZE);

    size_t i = 0;
    for (; i + LOGIN_DATA_LENGTH <= HOT_SIZE; i += LOGIN_DATA_LENGTH)
    {
        std::memcpy(&hotBuf[i], LOGIN_DATA, LOGIN_DATA_LENGTH);
    }
    std::memset(&hotBuf[i], ' ', HOT_SIZE - i);

    constexpr size_t msgsPerPass = HOT_SIZE / LOGIN_DATA_LENGTH;
    constexpr size_t hotMsgs = msgsPerPass * HOT_COUNT;

    LogonDataGetterHandler handler{};
    const auto duration = timer([&]
    {
        for (size_t iter = 0; iter < HOT_COUNT; ++iter)
        {
            for (size_t j = 0; j < msgsPerPass; ++j)
            {
                const std::span<const uint8_t> bytes(&hotBuf[j * LOGIN_DATA_LENGTH], LOGIN_DATA_LENGTH);
                const auto result = decoder.parse(bytes, handler);
                (void) result;
            }
        }
    });
    std::printf("sink = %llu\n", static_cast<unsigned long long>(handler.sink));
    report("LOGON DATA", duration, hotMsgs, LOGIN_DATA_LENGTH);
}

// NOS_HOT: hot-cache decode of NewOrderSingle (tokenization only, no getters).
static void benchNewOrderSingleHot()
{
    decoder::PayloadDecoder<FIXT_1_1> decoder;

    constexpr size_t HOT_SIZE  = 256 * 1024;
    constexpr size_t HOT_COUNT = 4096;
    auto hotBuf = std::make_unique<uint8_t[]>(HOT_SIZE);

    size_t i = 0;
    for (; i + NEW_ORDER_SINGLE_LENGTH <= HOT_SIZE; i += NEW_ORDER_SINGLE_LENGTH)
    {
        std::memcpy(&hotBuf[i], NEW_ORDER_SINGLE, NEW_ORDER_SINGLE_LENGTH);
    }
    std::memset(&hotBuf[i], ' ', HOT_SIZE - i);

    constexpr size_t msgsPerPass = HOT_SIZE / NEW_ORDER_SINGLE_LENGTH;

    for (int w = 0; w < 4; ++w)
    {
        for (size_t j = 0; j < msgsPerPass; ++j)
        {
            const std::span<const uint8_t> bytes(&hotBuf[j * NEW_ORDER_SINGLE_LENGTH], NEW_ORDER_SINGLE_LENGTH);
            (void) decoder.parse(bytes);
        }
    }

    constexpr size_t hotMsgs = msgsPerPass * HOT_COUNT;
    const auto duration = timer([&]
    {
        for (size_t iter = 0; iter < HOT_COUNT; ++iter)
        {
            for (size_t j = 0; j < msgsPerPass; ++j)
            {
                const std::span<const uint8_t> bytes(&hotBuf[j * NEW_ORDER_SINGLE_LENGTH], NEW_ORDER_SINGLE_LENGTH);
                (void) decoder.parse(bytes);
            }
        }
    });
    report("NOS HOT", duration, hotMsgs, NEW_ORDER_SINGLE_LENGTH);
}

// NOS_GETTERS: parse + apply every NewOrderSingleDecoder getter.
struct NewOrderSingleGetterHandler : org::limitless::fix::messages::MessageHandler<NewOrderSingleGetterHandler>
{
    using MessageHandler::handle;

    uint64_t sink = 0;

    org::limitless::fix::Result::Values handle(org::limitless::fix::messages::NewOrderSingleDecoder& order)
    {
        using namespace org::limitless::fix::messages;

        sink += order.clOrdID().value_or(std::string_view{}).size();
        sink += static_cast<uint64_t>(order.handlInst().value_or(HandlInst::AutoPrivate));
        sink += order.symbol().value_or(std::string_view{}).size();
        sink += static_cast<uint64_t>(order.side().value_or(Side::Buy));
        sink += static_cast<uint64_t>(order.transactTime().value_or(std::chrono::milliseconds{0}).count());
        sink += order.orderQty().value_or(0);
        sink += static_cast<uint64_t>(order.ordType().value_or(OrdType::Limit));
        sink += order.price().value_or(utils::FixedDecimal{}).mantissa();
        return org::limitless::fix::Result::Success;
    }
};

static void benchNewOrderSingleGetters()
{
    decoder::PayloadDecoder<FIXT_1_1> decoder;

    constexpr size_t HOT_SIZE  = 256 * 1024;
    constexpr size_t HOT_COUNT = 4096;
    auto hotBuf = std::make_unique<uint8_t[]>(HOT_SIZE);

    size_t i = 0;
    for (; i + NEW_ORDER_SINGLE_LENGTH <= HOT_SIZE; i += NEW_ORDER_SINGLE_LENGTH)
    {
        std::memcpy(&hotBuf[i], NEW_ORDER_SINGLE, NEW_ORDER_SINGLE_LENGTH);
    }
    std::memset(&hotBuf[i], ' ', HOT_SIZE - i);

    constexpr size_t msgsPerPass = HOT_SIZE / NEW_ORDER_SINGLE_LENGTH;
    constexpr size_t hotMsgs = msgsPerPass * HOT_COUNT;

    NewOrderSingleGetterHandler handler{};
    const auto duration = timer([&]
    {
        for (size_t iter = 0; iter < HOT_COUNT; ++iter)
        {
            for (size_t j = 0; j < msgsPerPass; ++j)
            {
                const std::span<const uint8_t> bytes(&hotBuf[j * NEW_ORDER_SINGLE_LENGTH], NEW_ORDER_SINGLE_LENGTH);
                (void) decoder.parse(bytes, handler);
            }
        }
    });
    std::printf("sink = %llu\n", static_cast<unsigned long long>(handler.sink));
    report("NOS GETTERS", duration, hotMsgs, NEW_ORDER_SINGLE_LENGTH);
}

// NOS_ENCODE: encode a NewOrderSingle message repeatedly.
static void benchNewOrderSingleEncode()
{
    using namespace org::limitless::fix::messages;

    constexpr size_t HOT_COUNT = 1'000'000;
    std::array<uint8_t, 256> buffer{};

    size_t encodedLength = 0;
    const auto duration = timer([&]
    {
        for (size_t i = 0; i < HOT_COUNT; ++i)
        {
            FixPayloadEncoder<FIXT_1_1, "TARGET", "SENDER"> encoder{};
            encoder.wrap(0, buffer);

            NewOrderSingleEncoder order{};
            encoder.wrapMessage(order)
                    .sequenceNumber(1)
                    .sendingTime(std::chrono::milliseconds{1'781'378'773'959})
                    .clOrdID("ORDER1")
                    .handlInst(HandlInst::AutoPrivate)
                    .symbol("AAPL")
                    .side(Side::Buy)
                    .transactTime(std::chrono::milliseconds{1'781'378'773'959})
                    .orderQty(100)
                    .ordType(OrdType::Limit)
                    .price(utils::FixedDecimal{15000, 0});

            encodedLength = encoder.encode(order);
        }
    });
    report("NOS ENCODE", duration, HOT_COUNT, encodedLength);
}

// ENCODE: encode a Logon message (with a 3-entry HopsRepeatingGroup) repeatedly.
static void benchEncode()
{
    using namespace org::limitless::fix::messages;

    constexpr size_t HOT_COUNT = 1'000'000;
    std::array<uint8_t, 256> buffer{};

    size_t encodedLength = 0;
    const auto duration = timer([&]
    {
        for (size_t i = 0; i < HOT_COUNT; ++i)
        {
            FixPayloadEncoder<FIXT_1_1, "SellSide_1", "Buyer"> encoder{};
            encoder.wrap(0, buffer);

            LogonEncoder logon{};
            encoder.wrapMessage(logon)
                    .sequenceNumber(1)
                    .sendingTime(std::chrono::milliseconds{1'781'378'773'959})
                    .encryptMethod(Encryption::None)
                    .heartbeatInterval(30);

            logon
                    .hops(3)
                    .next().hopCompID("HOP1").hopSendingTime(std::chrono::milliseconds{1'781'378'773'959}).hopRefID(1)
                    .next().hopCompID("HOP2").hopSendingTime(std::chrono::milliseconds{1'781'378'773'959}).hopRefID(2)
                    .next().hopCompID("HOP3").hopSendingTime(std::chrono::milliseconds{1'781'378'773'959}).hopRefID(3);

            encodedLength = encoder.encode(logon);
        }
    });
    report("LOGON ENC", duration, HOT_COUNT, encodedLength);
}

struct Benchmark
{
    std::string_view name;
    void (*run)();
};

static constexpr Benchmark BENCHMARKS[] = {
    {"logon-cold",    benchColdCache},
    {"logon-hot",     benchHotCache},
    {"logon-getters", benchGetters},
    {"logon-groups",  benchGroups},
    {"logon-data",    benchLogonData},
    {"logon-encode",  benchEncode},
    {"nos-hot",       benchNewOrderSingleHot},
    {"nos-getters",   benchNewOrderSingleGetters},
    {"nos-encode",    benchNewOrderSingleEncode},
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
    std::printf("Logon message length         = %zu bytes\n", LOGIN_LENGTH);
    std::printf("Logon+data message length    = %zu bytes\n", LOGIN_DATA_LENGTH);
    std::printf("NewOrderSingle message length = %zu bytes\n\n", NEW_ORDER_SINGLE_LENGTH);

    if (argc < 2)
    {
        for (const auto& [name, run] : BENCHMARKS)
        {
            run();
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