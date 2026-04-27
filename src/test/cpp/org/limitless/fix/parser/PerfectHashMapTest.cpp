//
// Created by Fredrik Dahlberg on 2026-04-27.
//

#include <gtest/gtest.h>

#include "org/limitless/fix/parser/PerfectHashMap.hpp"

namespace org::limitless::fix::parser {
struct Meta
{
    int32_t tag;
    int32_t type;
    bool required;
};

TEST(PerfechHash, Basics)
{
    static constexpr std::array<Entry<Meta>, 3> dictionary = {{
        {100, {}}, {500, {}}, {9999, {}}
    }};

    static constexpr PerfectHashMap map(dictionary);

    auto result = map.lookup(500);
    EXPECT_TRUE(result.has_value());
    }

TEST(PerfechHash, Duplicates)
{
    static constexpr std::array<Entry<Meta>, 3> dictionary = {{
        {100, {}}, {100, {}}, {9999, {}}
    }};

    EXPECT_THROW({
           PerfectHashMap map(dictionary);
       }, const char*); // We threw a "string literal", which is a const char*
}
}
