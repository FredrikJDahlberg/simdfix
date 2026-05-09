//
// Created by Fredrik Dahlberg on 2026-04-27.
//

#include <gtest/gtest.h>

#include "../../../../../../main/cpp/org/limitless/fix/utils/PerfectHashMap.hpp"
#include "../../../../../../main/cpp/org/limitless/fix/utils/Utils.hpp"

namespace org::limitless::fix::parser {

struct Meta
{
    int32_t tag;
    int32_t type;
    bool required;
};

TEST(PerfechHash, Basics)
{
    static constexpr std::array<Entry<Meta>, 3> entries = {{
        {100, {}}, {500, {}}, {9999, {}}
    }};

    static constexpr PerfectHashMap map(std::span{entries});
    const auto result = map.lookup(500);
    EXPECT_TRUE(result.has_value());
}

TEST(PerfechHash, Duplicates)
{
    static constexpr std::array<Entry<Meta>, 3> entries = {{
        {100, {}}, {101, {}}, {9999, {}}
    }};
    EXPECT_THROW({
                 static constexpr PerfectHashMap map(std::span{entries});
                 }, std::invalid_argument); // We threw a "string literal", which is a const char*
}

TEST(QuadSearch, Basics)
{
    const uint16_t values[] = {1, 2, 5, 7, 9, 12, 14, 15, 18, 23, 24, 29, 33, 35, 37, 40, 42 };
    ASSERT_EQ(-1, utils::fastLinearSearch(values, std::size(values), 47));
    ASSERT_EQ(11, utils::fastLinearSearch(values, std::size(values), 29));
}

}
