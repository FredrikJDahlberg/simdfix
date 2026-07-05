//
// Micro-benchmark for the FIX timestamp parsing path (utils/Conversions.hpp).
//
// Two parts:
//   1. Per-function cost of the timestamp/date/time parsers.
//   2. The date->days memoization effect: parsing is dominated by the
//      YYYYMMDD->days calendar arithmetic (~5.8 ns of ~8.8 ns), which repeats
//      whenever consecutive timestamps share a date (SendingTime/TransactTime
//      in a message, or every message in a day's feed). FieldDecoder memoizes
//      this; here we reproduce the effect on the factored utils helpers
//      (dateToEpochDays + timestampMillisFromDays) to show the headroom.
//
// Build a Release configuration for meaningful numbers.
//
#define NDEBUG 1

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "org/limitless/simdifx/utils/Conversions.hpp"

using namespace std::chrono;
using namespace org::limitless::simdifx;

namespace
{
// A padded timestamp/date buffer. Every buffer keeps >= 8 readable bytes past
// each field so the SWAR parsers can always load a full word.
struct Sample
{
    uint8_t bytes[32];
    uint32_t len;
};

Sample make(const char* str, uint32_t len)
{
    Sample s{};
    for (uint32_t i = 0; i < len; ++i)
    {
        s.bytes[i] = static_cast<uint8_t>(str[i]);
    }
    s.len = len;
    return s;
}

// A decoder-style cache built on the same utils helpers FieldDecoder uses.
struct DateCache
{
    uint64_t word{~0ull};
    int64_t days{0};

    int64_t parse(const uint8_t* data, uint32_t length)
    {
        if (length < utils::UTCTimestampShortLength)
        {
            return -1;
        }
        uint64_t w;
        std::memcpy(&w, data, sizeof(w));
        int64_t d;
        if (w == word)
        {
            d = days;
        }
        else
        {
            d = utils::dateToEpochDays(data);
            if (d < 0)
            {
                return -1;
            }
            word = w;
            days = d;
        }
        return utils::timestampMillisFromDays(data, length, d);
    }
};

template <typename Fn>
void run(const char* label, const std::array<Sample, 4>& samples, Fn fn, size_t iters)
{
    volatile int64_t sink = 0;
    const auto start = high_resolution_clock::now();
    for (size_t i = 0; i < iters; ++i)
    {
        const auto& s = samples[i & 3];
        sink += fn(s.bytes, s.len);
    }
    const auto dur = high_resolution_clock::now() - start;
    const double ns = static_cast<double>(duration_cast<nanoseconds>(dur).count()) / static_cast<double>(iters);
    std::printf("%-32s %7.3f ns/call\n", label, ns);
}
}

int main()
{
    constexpr size_t ITERS = 200'000'000;

    const std::array<Sample, 4> tsMillis = {
        make("20260613-19:26:13.959", 21), make("20190605-11:51:27.848", 21),
        make("20240101-00:00:00.000", 21), make("19991231-23:59:59.999", 21)};
    const std::array<Sample, 4> tsShort = {
        make("20260613-19:26:13", 17), make("20190605-11:51:27", 17),
        make("20240101-00:00:00", 17), make("19991231-23:59:59", 17)};
    const std::array<Sample, 4> timeOnly = {
        make("19:26:13.959", 12), make("11:51:27.848", 12),
        make("00:00:00.000", 12), make("23:59:59.999", 12)};
    const std::array<Sample, 4> dateOnly = {
        make("20260613", 8), make("20190605", 8),
        make("20240101", 8), make("19991231", 8)};

    // Same date, four times of day (the SendingTime/TransactTime-in-one-message
    // and same-trading-day-feed case).
    const std::array<Sample, 4> sameDate = {
        make("20260613-19:26:13.959", 21), make("20260613-09:01:02.100", 21),
        make("20260613-12:00:00.000", 21), make("20260613-23:59:59.999", 21)};

    std::printf("iters = %zu\n\n", ITERS);

    std::printf("-- per-function cost --\n");
    run("dateTimeToEpochUTC (21)", tsMillis, [](const uint8_t* d, uint32_t l) { return utils::dateTimeToEpochUTC(d, l); }, ITERS);
    run("dateTimeToEpochUTC (17)", tsShort, [](const uint8_t* d, uint32_t l) { return utils::dateTimeToEpochUTC(d, l); }, ITERS);
    run("timeOnlyToMillis   (12)", timeOnly, [](const uint8_t* d, uint32_t l) { return utils::timeOnlyToMillis(d, l); }, ITERS);
    run("dateOnlyToEpochUTC (8)", dateOnly, [](const uint8_t* d, uint32_t l) { return utils::dateOnlyToEpochUTC(d, l); }, ITERS);

    std::printf("\n-- date->days memoization (FieldDecoder-style cache) --\n");
    run("stateless  same-date", sameDate, [](const uint8_t* d, uint32_t l) { return utils::dateTimeToEpochUTC(d, l); }, ITERS);
    DateCache hot{};
    run("memoized   same-date", sameDate, [&](const uint8_t* d, uint32_t l) { return hot.parse(d, l); }, ITERS);
    run("stateless  vary-date", tsMillis, [](const uint8_t* d, uint32_t l) { return utils::dateTimeToEpochUTC(d, l); }, ITERS);
    DateCache cold{};
    run("memoized   vary-date", tsMillis, [&](const uint8_t* d, uint32_t l) { return cold.parse(d, l); }, ITERS);

    return 0;
}
