//
// Created by Fredrik Dahlberg on 2026-06-14.
//

#include <gtest/gtest.h>

#include "org/limitless/fix/decoder/PayloadDecoder.hpp"
#include "../../../../../../main/cpp/org/limitless/fix/detail/parser/FieldDecoder.hpp"
#include "org/limitless/fix/messages/FixTypes.hpp"
#include "org/limitless/fix/utils/Conversions.hpp"

namespace org::limitless::fix::decoder {

using namespace org::limitless::fix::detail::parser;

#define SOH "\x01"

TEST(FieldDecoder, GetInt32)
{
    const auto logout = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=62" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=10:11:12.123" SOH
        "9999=-12345" SOH "10=004" SOH);

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

    const auto negative = field.getInt32<9999, false, RecordType::Message>().value_or(0);
    const auto positive = field.getInt32<34, false, RecordType::Message>().value_or(0);
    EXPECT_EQ(-12345, negative);
    EXPECT_EQ(100101, positive);
}

TEST(FieldDecoder, GetFixedDecimal)
{
    const auto logout = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=73" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=10:11:12.123" SOH
        "9999=123.45" SOH "9998=-0.01" SOH "10=020" SOH);

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

    const auto positive = field.getFixedDecimal<9999, false, RecordType::Message>().value_or(utils::FixedDecimal{});
    const auto negative = field.getFixedDecimal<9998, false, RecordType::Message>().value_or(utils::FixedDecimal{});
    EXPECT_EQ(utils::FixedDecimal(12345, -2), positive);
    EXPECT_EQ(utils::FixedDecimal(-1, -2), negative);
}

TEST(FieldDecoder, GetFixedDecimalLong)
{
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=86" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=10:11:12.123" SOH
        "9999=123456.789012" SOH "9998=-1234.56789" SOH "10=213" SOH);

    PayloadDecoder<FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::Success, status);

    const auto fields = decoder.fields();
    std::vector<uint16_t> tags(fields.size());
    for (size_t i = 0; i < fields.size(); ++i)
    {
        tags[i] = fields[i].m_tag;
    }

    FieldDecoder field{message, fields, tags, static_cast<int32_t>(fields.size())};

    const auto positive = field.getFixedDecimal<9999, false, RecordType::Message>().value_or(utils::FixedDecimal{});
    const auto negative = field.getFixedDecimal<9998, false, RecordType::Message>().value_or(utils::FixedDecimal{});
    EXPECT_EQ(utils::FixedDecimal(123456789012LL, -6), positive);
    EXPECT_EQ(utils::FixedDecimal(-123456789LL, -5), negative);
}

TEST(FieldDecoder, GetFixedDecimalInvalidValue)
{
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=100" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=10:11:12.123" SOH
        "9999=.123" SOH "9998=123." SOH "9997=12.3.4" SOH "9996=12A.5" SOH "9995=-" SOH "10=097" SOH);

    PayloadDecoder<FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::Success, status);

    const auto fields = decoder.fields();
    std::vector<uint16_t> tags(fields.size());
    for (size_t i = 0; i < fields.size(); ++i)
    {
        tags[i] = fields[i].m_tag;
    }

    FieldDecoder field{message, fields, tags, static_cast<int32_t>(fields.size())};

    const auto leadingDot = field.getFixedDecimal<9999, false, RecordType::Message>();
    EXPECT_FALSE(leadingDot.has_value());
    EXPECT_EQ(Result::InvalidValue, leadingDot.error());

    const auto trailingDot = field.getFixedDecimal<9998, false, RecordType::Message>();
    EXPECT_FALSE(trailingDot.has_value());
    EXPECT_EQ(Result::InvalidValue, trailingDot.error());

    const auto multipleDots = field.getFixedDecimal<9997, false, RecordType::Message>();
    EXPECT_FALSE(multipleDots.has_value());
    EXPECT_EQ(Result::InvalidValue, multipleDots.error());

    const auto nonDigit = field.getFixedDecimal<9996, false, RecordType::Message>();
    EXPECT_FALSE(nonDigit.has_value());
    EXPECT_EQ(Result::InvalidValue, nonDigit.error());

    const auto signOnly = field.getFixedDecimal<9995, false, RecordType::Message>();
    EXPECT_FALSE(signOnly.has_value());
    EXPECT_EQ(Result::InvalidValue, signOnly.error());
}

TEST(FieldDecoder, GetUint32InvalidValue)
{
    const auto message = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=83" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=10:11:12.123" SOH
        "9999=ABC" SOH "9998=99999999999" SOH "9997=-" SOH "10=130" SOH);

    PayloadDecoder<FIXT_1_1> decoder;
    auto [processed, status] = decoder.parse(message);
    ASSERT_EQ(Result::Success, status);

    const auto fields = decoder.fields();
    std::vector<uint16_t> tags(fields.size());
    for (size_t i = 0; i < fields.size(); ++i)
    {
        tags[i] = fields[i].m_tag;
    }

    FieldDecoder field{message, fields, tags, static_cast<int32_t>(fields.size())};

    const auto nonDigit = field.getUint32<9999, false, RecordType::Message>();
    EXPECT_FALSE(nonDigit.has_value());
    EXPECT_EQ(Result::InvalidValue, nonDigit.error());

    const auto overflow = field.getUint32<9998, false, RecordType::Message>();
    EXPECT_FALSE(overflow.has_value());
    EXPECT_EQ(Result::InvalidLength, overflow.error());

    const auto signOnly = field.getInt32<9997, false, RecordType::Message>();
    EXPECT_FALSE(signOnly.has_value());
    EXPECT_EQ(Result::InvalidValue, signOnly.error());
}

TEST(FieldDecoder, GetInt32MissingField)
{
    const auto logout = utils::makeSpan(
        "8=FIXT.1.1" SOH "9=50" SOH "35=5" SOH "49=Buyer" SOH "56=Seller" SOH "34=100101" SOH "52=10:11:12.123" SOH
        "10=179" SOH);

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

    const auto optional = field.getInt32<9999, false, RecordType::Message>();
    const auto required = field.getInt32<9999, true, RecordType::Message>();
    EXPECT_FALSE(optional.has_value());
    EXPECT_EQ(Result::RequiredFieldMissing, required.error());
}

}