//
// Created by Fredrik Dahlberg on 2026-04-25.
//

#ifndef SIMD_FIX_PARSER_STATUS_HPP
#define SIMD_FIX_PARSER_STATUS_HPP

#include <string_view>

namespace org::limitless::fix::decoder {

struct Result
{
    enum Values
    {
        Success,
        MessageFragment,
        InvalidBeginString,
        InvalidCheckSumTag,
        InvalidBodyLengthTag,
        InvalidBodyLength,
        InvalidCheckSum,
        InvalidTargetCompTag,
        InvalidTargetCompId,
        InvalidSenderCompTag,
        InvalidSenderCompId,
        InvalidSequenceNumber,
        InvalidMessageTypeTag,
        InvalidMessageType,
        InvalidSendingTime,
        RequiredFieldMissing,
        InvalidLength,
        NullValue
    };

    uint32_t m_processed;
    Values m_value;

    constexpr Result() : m_value{Success} {}

    explicit constexpr Result(const Values value) : m_value{value} {}

    explicit constexpr Result(const std::string_view name) : m_value{Success}
    {
        for (int i = 0; i < 7; ++i)
        {
            if (Names[i] == name)
            {
                m_value = static_cast<Values>(i);
                return;
            }
        }
    }

    constexpr Result(const uint32_t processed, const Values value) : m_processed{processed}, m_value{value}
    {
    }

    [[nodiscard]] constexpr std::string_view name() const
    {
        return Names[m_value];
    }

    constexpr bool operator==(const Values value) const
    {
        return m_value == value;
    }

    constexpr bool operator!=(const Values value) const
    {
        return m_value != value;
    }

    static constexpr std::string_view Names[] = {
        "Success",
        "MessageFragment",
        "InvalidBeginString",
        "InvalidCheckSumTag",
        "InvalidBodyLengthTag",
        "InvalidBodyLength",
        "InvalidCheckSum",
        "InvalidTargetCompTag",
        "InvalidTargetCompId",
        "InvalidSenderCompTag",
        "InvalidSenderCompId",
        "InvalidSequenceNumber",
        "InvalidMessageTypeTag",
        "InvalidMessageType",
        "InvalidSendingTime",
        "RequiredFieldMissing",
        "InvalidLength",
        "NullValue"
    };
};
}

#endif //SIMD_FIX_PARSER_STATUS_HPP
