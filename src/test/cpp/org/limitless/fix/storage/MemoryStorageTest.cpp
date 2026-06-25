//
// Created by Fredrik Dahlberg on 2026-06-25.
//

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <span>

#include "org/limitless/fix/storage/MemoryStorage.hpp"

namespace org::limitless::fix::storage
{

static std::span<const uint8_t> bytes(const std::initializer_list<uint8_t>& data)
{
    static std::array<std::array<uint8_t, 16>, 8> pools{};
    static size_t next = 0;
    auto& pool = pools.at(next++ % pools.size());
    std::copy(data.begin(), data.end(), pool.begin());
    return {pool.data(), data.size()};
}

TEST(MemoryStorage, SatisfiesStrategyConcept)
{
    static_assert(FixStorageStrategy<MemoryStorage>);
}

TEST(MemoryStorage, AppendThenGetMessage)
{
    MemoryStorage store;
    store.appendMessage(1, 'A', bytes({1, 2, 3}));
    store.appendMessage(2, 'D', bytes({4, 5}));

    const auto first = store.getMessage(1);
    EXPECT_EQ(1u, first.m_sequenceNumber);
    EXPECT_EQ('A', first.m_messageType);
    EXPECT_EQ(3u, first.m_payloadSize);
    ASSERT_EQ(3u, first.m_payload.size());
    EXPECT_EQ(1, first.m_payload[0]);
    EXPECT_EQ(3, first.m_payload[2]);

    const auto second = store.getMessage(2);
    EXPECT_EQ('D', second.m_messageType);
    EXPECT_EQ(2u, second.m_payload.size());
    EXPECT_EQ(5, second.m_payload[1]);
}

TEST(MemoryStorage, GetMessageMissReturnsEmpty)
{
    MemoryStorage store;
    store.appendMessage(5, 'A', bytes({1}));

    EXPECT_TRUE(store.getMessage(4).m_payload.empty());   // before base
    EXPECT_TRUE(store.getMessage(6).m_payload.empty());   // after last
    EXPECT_TRUE(store.getMessage(0).m_payload.empty());
}

TEST(MemoryStorage, EarlyPayloadStaysValidAfterLaterAppends)
{
    MemoryStorage store;
    store.appendMessage(1, 'A', bytes({10, 11, 12}));
    for (uint32_t seq = 2; seq <= 50; ++seq)
    {
        store.appendMessage(seq, 'D', bytes({static_cast<uint8_t>(seq)}));
    }

    // The span handed out for message 1 must still point at valid bytes.
    const auto first = store.getMessage(1);
    ASSERT_EQ(3u, first.m_payload.size());
    EXPECT_EQ(10, first.m_payload[0]);
    EXPECT_EQ(12, first.m_payload[2]);
}

TEST(MemoryStorage, GetMessagesRange)
{
    MemoryStorage store;
    store.appendMessage(1, 'A', bytes({1}));
    store.appendMessage(2, 'D', bytes({2}));
    store.appendMessage(3, '8', bytes({3}));
    store.appendMessage(4, '8', bytes({4}));

    const auto range = store.getMessages(2, 3);
    ASSERT_EQ(2u, range.size());
    EXPECT_EQ(2u, range[0].m_sequenceNumber);
    EXPECT_EQ(3u, range[1].m_sequenceNumber);
}

TEST(MemoryStorage, GetMessagesClampsToStored)
{
    MemoryStorage store;
    store.appendMessage(1, 'A', bytes({1}));
    store.appendMessage(2, 'D', bytes({2}));
    store.appendMessage(3, '8', bytes({3}));

    const auto clampedHigh = store.getMessages(2, 100);
    ASSERT_EQ(2u, clampedHigh.size());
    EXPECT_EQ(2u, clampedHigh[0].m_sequenceNumber);
    EXPECT_EQ(3u, clampedHigh[1].m_sequenceNumber);

    const auto all = store.getMessages(0, 100);
    EXPECT_EQ(3u, all.size());
}

TEST(MemoryStorage, GetMessagesEmptyCases)
{
    MemoryStorage store;
    EXPECT_TRUE(store.getMessages(1, 3).empty());   // nothing stored

    store.appendMessage(10, 'A', bytes({1}));
    EXPECT_TRUE(store.getMessages(3, 1).empty());    // inverted range
    EXPECT_TRUE(store.getMessages(1, 9).empty());    // entirely before base
    EXPECT_TRUE(store.getMessages(11, 20).empty());  // entirely after last
}

TEST(MemoryStorage, ClearResetsStore)
{
    MemoryStorage store;
    store.appendMessage(1, 'A', bytes({1}));
    store.appendMessage(2, 'D', bytes({2}));

    store.clear();
    EXPECT_TRUE(store.getMessage(1).m_payload.empty());
    EXPECT_TRUE(store.getMessages(1, 2).empty());

    // Usable again, rebasing on the next sequence number seen.
    store.appendMessage(7, '8', bytes({9}));
    const auto message = store.getMessage(7);
    EXPECT_EQ(7u, message.m_sequenceNumber);
    ASSERT_EQ(1u, message.m_payload.size());
    EXPECT_EQ(9, message.m_payload[0]);
}

}
