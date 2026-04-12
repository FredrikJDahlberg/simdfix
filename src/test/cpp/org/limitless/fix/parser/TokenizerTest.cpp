//
// Created by Fredrik Dahlberg on 2026-04-11.
//

#include <gtest/gtest.h>

#include "org/limitless/fix/parser/Tokenizer.h"

namespace org::limitless::fix::parser {

void dump(size_t length, const char* buffer)
{
    for (int i = 0; i < length; ++i)
    {
        switch (auto ch = buffer[i])
        {
            case 1:
                std::putchar('|');
                break;
            default:
                std::putchar(std::isprint(ch) ? ch : '?');
                break;
        }
    }
}

TEST(Tokenizer, Basics)
{
    #define SOH "\x01"
    constexpr uint8_t message[] =
        "8=FIXT.1.1" SOH
        "9=116" SOH
        "35=A" SOH
        "49=BuySide" SOH
        "56=SellSide_1" SOH
        "34=1" SOH
        "52=20190605-11:51:27.848" SOH
        "1128=9" SOH
        "98=0" SOH
        "108=30" SOH
        "141=Y" SOH
        "553=Username" SOH
        "554=Password" SOH
        "1137=9" SOH
        "10=079" SOH
        "  ";
    Tokenizer tokenizer;
    tokenizer.scan(message, sizeof(message) - 1);
}

}