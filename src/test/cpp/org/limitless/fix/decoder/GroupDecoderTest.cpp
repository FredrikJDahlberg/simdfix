//
// Created by Fredrik Dahlberg on 2026-06-10.
//

#include <gtest/gtest.h>

#include "org/limitless/fix/decoder/PayloadDecoder.hpp"
#include "../../../../../../main/cpp/org/limitless/fix/decoder/FieldDecoder.hpp"
#include "org/limitless/fix/decoder/GroupDecoder.hpp"
#include "org/limitless/fix/utils/Utils.hpp"

namespace org::limitless::fix::decoder {

#define SOH "\x01"

static constexpr auto Empty = std::string_view{};

// Group: NoHops(627)=2, repeats of HopSendingTime(629)/HopCompID(628)
TEST(GroupDecoder, ScopesFieldLookupToCurrentRepeat)
{
    const auto logout = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=84" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=10:11:12.123" SOH
        "627=2" SOH "629=10" SOH "628=12" SOH "629=37" SOH "628=20" SOH "10=211" SOH);

    PayloadDecoder decoder{Protocol::FIXT_1_1};
    auto [processed, status] = decoder.parse(logout);
    ASSERT_EQ(Result::Success, status);

    const auto tokens = decoder.tokens();
    std::vector<uint16_t> tags(tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        tags[i] = tokens[i].m_tag;
    }

    FieldDecoder field{logout, tokens, tags, static_cast<int32_t>(tokens.size())};
    GroupDecoder group{field};
    group.wrap(627);
    ASSERT_EQ(2u, group.count());

    ASSERT_TRUE(group.hasNext());
    group.next();
    {
        const auto sendingTime = field.getUint32<629, false, ParentType::Group>().value_or(0);
        const auto compID = field.getString<628, false, ParentType::Group>().value_or(Empty);
        EXPECT_EQ(10u, sendingTime);
        EXPECT_EQ("12", compID);
    }

    ASSERT_TRUE(group.hasNext());
    group.next();
    {
        const auto sendingTime = field.getUint32<629, false, ParentType::Group>().value_or(0);
        const auto compID = field.getString<628, false, ParentType::Group>().value_or(Empty);
        EXPECT_EQ(37u, sendingTime);
        EXPECT_EQ("20", compID);
    }

    EXPECT_FALSE(group.hasNext());
    group.clear();

    // Outside any group scope, lookups search the whole message again.
    const auto firstSendingTime = field.getUint32<629, false, ParentType::Message>().value_or(0);
    EXPECT_EQ(10u, firstSendingTime);
}

// Re-wrapping after a full iteration (without calling clear()) leaves the
// final repeat's scope pushed but never popped. Repeating this more times
// than MaxGroupDepth would overflow the scope stack and corrupt m_scopeDepth.
TEST(GroupDecoder, RewrapWithoutClearDoesNotLeakScopeDepth)
{
    const auto logout = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=84" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=10:11:12.123" SOH
        "627=2" SOH "629=10" SOH "628=12" SOH "629=37" SOH "628=20" SOH "10=211" SOH);

    PayloadDecoder decoder{Protocol::FIXT_1_1};
    auto [processed, status] = decoder.parse(logout);
    ASSERT_EQ(Result::Success, status);

    const auto tokens = decoder.tokens();
    std::vector<uint16_t> tags(tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        tags[i] = tokens[i].m_tag;
    }

    FieldDecoder field{logout, tokens, tags, static_cast<int32_t>(tokens.size())};
    GroupDecoder group{field};

    // MaxGroupDepth is 8; iterate well beyond that without clear() to ensure
    // wrap() pops the leftover scope from the previous iteration.
    for (int i = 0; i < 16; ++i)
    {
        group.wrap(627);
        ASSERT_EQ(2u, group.count());

        ASSERT_TRUE(group.hasNext());
        group.next();
        {
            const auto sendingTime = field.getUint32<629, false, ParentType::Group>().value_or(0);
            const auto compID = field.getString<628, false, ParentType::Group>().value_or(Empty);
            EXPECT_EQ(10u, sendingTime);
            EXPECT_EQ("12", compID);
        }

        ASSERT_TRUE(group.hasNext());
        group.next();
        {
            const auto sendingTime = field.getUint32<629, false, ParentType::Group>().value_or(0);
            const auto compID = field.getString<628, false, ParentType::Group>().value_or(Empty);
            EXPECT_EQ(37u, sendingTime);
            EXPECT_EQ("20", compID);
        }

        EXPECT_FALSE(group.hasNext());
    }
}

}
