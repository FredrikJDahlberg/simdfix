//
// Created by Fredrik Dahlberg on 2026-06-10.
//

#include <gtest/gtest.h>

#include "org/limitless/simdifx/decoder/PayloadDecoder.hpp"
#include "org/limitless/simdifx/detail/parser/FieldDecoder.hpp"
#include "org/limitless/simdifx/decoder/GroupDecoder.hpp"
#include "org/limitless/simdifx/utils/Conversions.hpp"

#include "org/limitless/simdifx/generated/messages/FixTypes.hpp"

namespace org::limitless::simdifx::decoder {

using namespace org::limitless::simdifx::generated::config;

#define SOH "\x01"

static constexpr auto Empty = std::string_view{};

// Group: NoHops(627)=2, repeats of HopSendingTime(629)/HopCompID(628)
TEST(GroupDecoder, ScopesFieldLookupToCurrentRepeat)
{
    const auto logout = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=84" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=10:11:12.123" SOH
        "627=2" SOH "629=10" SOH "628=12" SOH "629=37" SOH "628=20" SOH "10=211" SOH);

    PayloadDecoder<FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(logout);
    ASSERT_EQ(Result::Success, status);

    const auto fields = decoder.fields();
    std::vector<uint16_t> tags(fields.size());
    for (size_t i = 0; i < fields.size(); ++i)
    {
        tags[i] = fields[i].m_tag;
    }

    FieldDecoder field{logout, fields, tags, static_cast<int32_t>(fields.size())};
    GroupDecoder group{field};
    group.wrap<627>();
    ASSERT_EQ(2u, group.count());

    ASSERT_TRUE(group.hasNext());
    group.next();
    {
        const auto sendingTime = field.getUint32<629, false, RecordType::Group>().value_or(0);
        const auto compID = field.getString<628, false, RecordType::Group>().value_or(Empty);
        EXPECT_EQ(10u, sendingTime);
        EXPECT_EQ("12", compID);
    }

    ASSERT_TRUE(group.hasNext());
    group.next();
    {
        const auto sendingTime = field.getUint32<629, false, RecordType::Group>().value_or(0);
        const auto compID = field.getString<628, false, RecordType::Group>().value_or(Empty);
        EXPECT_EQ(37u, sendingTime);
        EXPECT_EQ("20", compID);
    }

    EXPECT_FALSE(group.hasNext());
    group.clear();

    // Outside any group scope, lookups search the whole message again.
    const auto firstSendingTime = field.getUint32<629, false, RecordType::Message>().value_or(0);
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

    PayloadDecoder<FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(logout);
    ASSERT_EQ(Result::Success, status);

    const auto fields = decoder.fields();
    std::vector<uint16_t> tags(fields.size());
    for (size_t i = 0; i < fields.size(); ++i)
    {
        tags[i] = fields[i].m_tag;
    }

    FieldDecoder field{logout, fields, tags, static_cast<int32_t>(fields.size())};
    GroupDecoder group{field};

    // MaxGroupDepth is 8; iterate well beyond that without clear() to ensure
    // wrap() pops the leftover scope from the previous iteration.
    for (int i = 0; i < 16; ++i)
    {
        group.wrap<627>();
        ASSERT_EQ(2u, group.count());

        ASSERT_TRUE(group.hasNext());
        group.next();
        {
            const auto sendingTime = field.getUint32<629, false, RecordType::Group>().value_or(0);
            const auto compID = field.getString<628, false, RecordType::Group>().value_or(Empty);
            EXPECT_EQ(10u, sendingTime);
            EXPECT_EQ("12", compID);
        }

        ASSERT_TRUE(group.hasNext());
        group.next();
        {
            const auto sendingTime = field.getUint32<629, false, RecordType::Group>().value_or(0);
            const auto compID = field.getString<628, false, RecordType::Group>().value_or(Empty);
            EXPECT_EQ(37u, sendingTime);
            EXPECT_EQ("20", compID);
        }

        EXPECT_FALSE(group.hasNext());
    }
}

}
