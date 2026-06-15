//
// Created by Fredrik Dahlberg on 2026-06-14.
//

#include <gtest/gtest.h>

#include "org/limitless/fix/decoder/PayloadDecoder.hpp"
#include "org/limitless/fix/decoder/FieldDecoder.hpp"
#include "org/limitless/fix/utils/Utils.hpp"

namespace org::limitless::fix::decoder {

#define SOH "\x01"

TEST(FieldDecoder, GetInt32)
{
    const auto logout = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=62" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=10:11:12.123" SOH
        "9999=-12345" SOH "10=004" SOH);

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

    const auto negative = field.getInt32<9999, false, RecordType::Message>().value_or(0);
    const auto positive = field.getInt32<34, false, RecordType::Message>().value_or(0);
    EXPECT_EQ(-12345, negative);
    EXPECT_EQ(100101, positive);
}

TEST(FieldDecoder, GetInt32MissingField)
{
    const auto logout = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=50" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=10:11:12.123" SOH
        "10=179" SOH);

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

    const auto optional = field.getInt32<9999, false, RecordType::Message>();
    const auto required = field.getInt32<9999, true, RecordType::Message>();
    EXPECT_FALSE(optional.has_value());
    EXPECT_EQ(Result::RequiredFieldMissing, required.error());
}

}