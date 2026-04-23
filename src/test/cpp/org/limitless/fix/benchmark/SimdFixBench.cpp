//
// Created by Fredrik Dahlberg on 2026-04-20.
//
#define NDEBUG 1

#include <memory>
#include <cstddef>

#include "org/limitless/fix/parser/Uint8x16.hpp"
#include "org/limitless/fix/parser/Tokenizer.hpp"

#define SOH "\x01"
static constexpr std::uint8_t MESSAGE1[] =
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
    "10=147" SOH
    // next message
    "                      ";

int main(int argc, char** argv)
{
    using namespace org::limitless::fix::parser;
    Tokenizer tokenizer;

    constexpr size_t SIZE = 1024*1024*1024ull;
    auto buffer = std::make_unique<std::uint8_t[]>(SIZE);
    for (size_t i = 0; i < SIZE - sizeof(MESSAGE1); i += sizeof(MESSAGE1))
    {
        memcpy(&buffer[i], MESSAGE1, sizeof(MESSAGE1));
    }

    const auto start = std::chrono::high_resolution_clock::now();
    uint8_t checkSum;
    for (size_t i = 0; i < SIZE - sizeof(MESSAGE1); i += sizeof(MESSAGE1))
    {
        const std::span<const uint8_t> bytes(&buffer[i], sizeof(MESSAGE1));
        tokenizer.scan(bytes, checkSum);
    }
    const auto end = std::chrono::high_resolution_clock::now();
    const auto  duration = std::chrono::nanoseconds(end - start);
    std::printf("Duration = %lld ms\n", duration.count()/1'000'000);

    constexpr auto bytesSent = static_cast<double>(SIZE) - sizeof(MESSAGE1);
    const auto seconds = std::chrono::duration<double>(duration).count();
    const auto gigaBytesPerSecond = bytesSent / SIZE / seconds;
    std::printf("GigaBytes per second = %.3f\n", gigaBytesPerSecond);
    return 0;
}
