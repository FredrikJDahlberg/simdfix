//
// Created by Fredrik Dahlberg on 2026-04-20.
//
#include <cstdint>

#include "org/limitless/fix/parser/Block.hpp"
#include "org/limitless/fix/parser/Tokenizer.hpp"

#define SOH "\x01"
static constexpr std::uint8_t MESSAGE1[] =
    "8=FIXT.1.1" SOH
    "9=116" SOH
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
    "10=079" SOH
    // next message
    "                      ";



static uint8_t buffer[4096*1000];

int main(int argc, char** argv)
{
    using namespace org::limitless::fix::parser;
    Tokenizer tokenizer;

    const uint64_t sum = 1'000'000 * sizeof(MESSAGE1);
    for (int i = 0; i < sizeof(buffer); i += sizeof(MESSAGE1))
    {
        memcpy(buffer + i, MESSAGE1, sizeof(MESSAGE1));
    }
    std::printf("count = %lu\n", sizeof(buffer)/sizeof(MESSAGE1));

    const auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < sizeof(buffer); i += sizeof(MESSAGE1))
    {
        tokenizer.scan(buffer + i, sizeof(MESSAGE1) - 1);
    }
    const auto end = std::chrono::high_resolution_clock::now();
    const auto  duration = std::chrono::nanoseconds(end - start);
    double gbPerSec = 1'000'000'000.0*sizeof(buffer)/(duration.count()*1024*1024*1024);
    std::printf("GB per second = %.2g\n", gbPerSec);
    return 0;
}